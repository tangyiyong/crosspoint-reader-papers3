#include "ImageBlock.h"

#include <GfxRenderer.h>
#include <Logging.h>
#include <Serialization.h>
#include <esp_heap_caps.h>

#include "../converters/DitherUtils.h"
#include "../converters/ImageDecoderFactory.h"

// Cache file format:
// - uint16_t width
// - uint16_t height
// - uint8_t pixels[...] - 2 bits per pixel, packed (4 pixels per byte), row-major order

ImageBlock::ImageBlock(const std::string& imagePath, int16_t width, int16_t height)
    : imagePath(imagePath), width(width), height(height) {}

bool ImageBlock::imageExists() const { return Storage.exists(imagePath.c_str()); }

namespace {

std::string getCachePath(const std::string& imagePath) {
  // Replace extension with .pxc (pixel cache)
  size_t dotPos = imagePath.rfind('.');
  if (dotPos != std::string::npos) {
    return imagePath.substr(0, dotPos) + ".pxc";
  }
  return imagePath + ".pxc";
}

uint8_t* allocateCacheReadBuffer(const size_t bytes) {
#ifdef BOARD_HAS_PSRAM
  return static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
  return static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
#endif
}

void freeCacheReadBuffer(void* ptr) {
  if (ptr) heap_caps_free(ptr);
}

struct ImageRenderScope {
  GfxRenderer& renderer;
  bool active;

  ImageRenderScope(GfxRenderer& renderer, const bool active) : renderer(renderer), active(active) {
    if (active) renderer.beginImageRender();
  }

  ~ImageRenderScope() {
    if (active) renderer.endImageRender();
  }
};

bool renderFromCache(GfxRenderer& renderer, const std::string& cachePath, int x, int y, int expectedWidth,
                     int expectedHeight) {
  FsFile cacheFile;
  if (!Storage.openFileForRead("IMG", cachePath, cacheFile)) {
    return false;
  }

  uint16_t cachedWidth, cachedHeight;
  if (cacheFile.read(&cachedWidth, 2) != 2 || cacheFile.read(&cachedHeight, 2) != 2) {
    return false;
  }

  // Verify dimensions are close (allow 1 pixel tolerance for rounding differences)
  int widthDiff = abs(cachedWidth - expectedWidth);
  int heightDiff = abs(cachedHeight - expectedHeight);
  if (widthDiff > 1 || heightDiff > 1) {
    LOG_ERR("IMG", "Cache dimension mismatch: %dx%d vs %dx%d", cachedWidth, cachedHeight, expectedWidth,
            expectedHeight);
    return false;
  }

  // Use cached dimensions for rendering (they're the actual decoded size)
  expectedWidth = cachedWidth;
  expectedHeight = cachedHeight;

  LOG_DBG("IMG", "Loading from cache: %s (%dx%d)", cachePath.c_str(), cachedWidth, cachedHeight);

  // Read several rows per SD access without holding the full image.
  const int bytesPerRow = (cachedWidth + 3) / 4;  // 2 bits per pixel, 4 pixels per byte
  int rowsPerRead = 4096 / bytesPerRow;
  if (rowsPerRead < 1) rowsPerRead = 1;
  if (rowsPerRead > cachedHeight) rowsPerRead = cachedHeight;
  uint8_t* readBuffer = allocateCacheReadBuffer(static_cast<size_t>(rowsPerRead) * bytesPerRow);
  if (!readBuffer && rowsPerRead > 1) {
    rowsPerRead = 1;
    readBuffer = allocateCacheReadBuffer(static_cast<size_t>(bytesPerRow));
  }
  if (!readBuffer) {
    LOG_ERR("IMG", "Failed to allocate row buffer");
    return false;
  }

  int rowsInBuffer = 0;
  int bufferRow = 0;
  for (int row = 0; row < cachedHeight; row++) {
    if (bufferRow >= rowsInBuffer) {
      const int toRead = (cachedHeight - row < rowsPerRead) ? (cachedHeight - row) : rowsPerRead;
      const size_t bytes = static_cast<size_t>(toRead) * bytesPerRow;
      if (cacheFile.read(readBuffer, bytes) != static_cast<int>(bytes)) {
        LOG_ERR("IMG", "Cache read error at row %d", row);
        freeCacheReadBuffer(readBuffer);
        return false;
      }
      rowsInBuffer = toRead;
      bufferRow = 0;
    }

    const uint8_t* rowBuffer = readBuffer + static_cast<size_t>(bufferRow) * bytesPerRow;
    bufferRow++;

    const int destY = y + row;
    for (int col = 0; col < cachedWidth; col++) {
      const int byteIdx = col >> 2;
      const int bitShift = 6 - (col & 3) * 2;  // MSB first within byte
      const uint8_t pixelValue = (rowBuffer[byteIdx] >> bitShift) & 0x03;

      drawPixelWithRenderMode(renderer, x + col, destY, pixelValue);
    }
  }

  freeCacheReadBuffer(readBuffer);
  LOG_DBG("IMG", "Cache render complete");
  return true;
}

}  // namespace

void ImageBlock::render(GfxRenderer& renderer, const int x, const int y) {
  if (renderer.isFontCacheScanning()) {
    return;
  }

  LOG_DBG("IMG", "Rendering image at %d,%d: %s (%dx%d)", x, y, imagePath.c_str(), width, height);

  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();

  // Bounds check render position using logical screen dimensions
  if (x < 0 || y < 0 || x + width > screenWidth || y + height > screenHeight) {
    LOG_ERR("IMG", "Invalid render position: (%d,%d) size (%dx%d) screen (%dx%d)", x, y, width, height, screenWidth,
            screenHeight);
    return;
  }

  const bool skipInversion = renderer.isDarkMode() && !renderer.shouldInvertImagesInDarkMode();
  ImageRenderScope imageScope(renderer, skipInversion);
  if (skipInversion) {
    renderer.fillRect(x, y, width, height, false);
  }

  // Try to render from cache first
  std::string cachePath = getCachePath(imagePath);
  if (renderFromCache(renderer, cachePath, x, y, width, height)) {
    return;  // Successfully rendered from cache
  }

  // No cache - need to decode the image
  // Check if image file exists
  FsFile file;
  if (!Storage.openFileForRead("IMG", imagePath, file)) {
    LOG_ERR("IMG", "Image file not found: %s", imagePath.c_str());
    return;
  }
  size_t fileSize = file.size();
  file.close();

  if (fileSize == 0) {
    LOG_ERR("IMG", "Image file is empty: %s", imagePath.c_str());
    return;
  }

  LOG_DBG("IMG", "Decoding and caching: %s", imagePath.c_str());

  RenderConfig config;
  config.x = x;
  config.y = y;
  config.maxWidth = width;
  config.maxHeight = height;
  config.useGrayscale = true;
  config.useDithering = true;
  config.performanceMode = false;
  config.useExactDimensions = true;  // Use pre-calculated dimensions to avoid rounding mismatches
  config.cachePath = cachePath;      // Enable caching during decode

  ImageToFramebufferDecoder* decoder = ImageDecoderFactory::getDecoder(imagePath);
  if (!decoder) {
    LOG_ERR("IMG", "No decoder found for image: %s", imagePath.c_str());
    return;
  }

  LOG_DBG("IMG", "Using %s decoder", decoder->getFormatName());

  bool success = decoder->decodeToFramebuffer(imagePath, renderer, config);
  if (!success) {
    LOG_ERR("IMG", "Failed to decode image: %s", imagePath.c_str());
    return;
  }

  LOG_DBG("IMG", "Decode successful");
}

bool ImageBlock::serialize(FsFile& file) {
  serialization::writeString(file, imagePath);
  serialization::writePod(file, width);
  serialization::writePod(file, height);
  return true;
}

std::unique_ptr<ImageBlock> ImageBlock::deserialize(FsFile& file) {
  std::string path;
  serialization::readString(file, path);
  int16_t w, h;
  serialization::readPod(file, w);
  serialization::readPod(file, h);
  return std::unique_ptr<ImageBlock>(new ImageBlock(path, w, h));
}

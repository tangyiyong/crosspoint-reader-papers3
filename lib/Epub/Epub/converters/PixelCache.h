#pragma once

#include <HalStorage.h>
#include <Logging.h>
#include <stdint.h>
#include <esp_heap_caps.h>

#include <cstdlib>
#include <cstring>
#include <string>

// Streaming cache writer for 2-bit pixels (4 levels). Packs 4 pixels per byte,
// MSB first.
struct PixelCache {
  uint8_t* buffer;
  uint8_t* zeroRow;
  int width;
  int height;
  int bytesPerRow;
  int originX;      // config.x - to convert screen coords to cache coords
  int originY;      // config.y
  int bandRows;     // rows held in the band buffer
  int bandStart;    // image-local row index of band buffer row 0
  int flushedRows;  // image-local rows already written to file
  FsFile file;
  std::string cachePathStr;
  bool ok;

  PixelCache()
      : buffer(nullptr),
        zeroRow(nullptr),
        width(0),
        height(0),
        bytesPerRow(0),
        originX(0),
        originY(0),
        bandRows(0),
        bandStart(0),
        flushedRows(0),
        ok(false) {}
  PixelCache(const PixelCache&) = delete;
  PixelCache& operator=(const PixelCache&) = delete;

  static constexpr int MIN_BAND_ROWS = 16;
  static constexpr size_t MAX_BAND_BYTES = 24 * 1024;

  static uint8_t* allocateBandBuffer(const size_t bytes) {
#ifdef BOARD_HAS_PSRAM
    return static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
    return static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
#endif
  }

  static void freeBandBuffer(void* ptr) {
    if (ptr) heap_caps_free(ptr);
  }

  bool begin(const std::string& cachePath, int w, int h, int ox, int oy, int maxBlockDstRows) {
    width = w;
    height = h;
    originX = ox;
    originY = oy;
    bytesPerRow = (w + 3) / 4;  // 2 bits per pixel, 4 pixels per byte
    bandStart = 0;
    flushedRows = 0;
    ok = false;

    int wantRows = maxBlockDstRows + 2;
    if (wantRows < MIN_BAND_ROWS) wantRows = MIN_BAND_ROWS;
    if (wantRows > h) wantRows = h;

    size_t maxRowsByMem = MAX_BAND_BYTES / static_cast<size_t>(bytesPerRow);
    if (maxRowsByMem < 1) maxRowsByMem = 1;
    if (static_cast<size_t>(wantRows) > maxRowsByMem) wantRows = static_cast<int>(maxRowsByMem);

    // The band must fit the tallest single decoder callback, or rows would be
    // dropped. If it cannot, fall back to no cache for this decode.
    if (wantRows < maxBlockDstRows) {
      LOG_ERR("IMG", "Cache band too small (%d < %d rows) for %dx%d", wantRows, maxBlockDstRows, w, h);
      return false;
    }

    bandRows = wantRows;
    const size_t bufferSize = static_cast<size_t>(bandRows + 1) * static_cast<size_t>(bytesPerRow);
    buffer = allocateBandBuffer(bufferSize);
    if (!buffer) {
      LOG_ERR("IMG", "Failed to allocate cache band: %u bytes", static_cast<unsigned>(bufferSize));
      return false;
    }
    memset(buffer, 0, bufferSize);
    zeroRow = buffer + static_cast<size_t>(bandRows) * bytesPerRow;

    if (!Storage.openFileForWrite("IMG", cachePath, file)) {
      LOG_ERR("IMG", "Failed to open cache file for writing: %s", cachePath.c_str());
      freeBandBuffer(buffer);
      buffer = nullptr;
      zeroRow = nullptr;
      return false;
    }

    cachePathStr = cachePath;

    const uint16_t w16 = static_cast<uint16_t>(w);
    const uint16_t h16 = static_cast<uint16_t>(h);
    if (file.write(&w16, 2) != 2 || file.write(&h16, 2) != 2) {
      LOG_ERR("IMG", "Failed to write cache header: %s", cachePath.c_str());
      abort();
      return false;
    }

    ok = true;
    LOG_DBG("IMG", "Cache stream started: %s (%dx%d, band %d rows)", cachePath.c_str(), w, h, bandRows);
    return true;
  }

  void setPixel(int screenX, int screenY, uint8_t value) {
    if (!ok || !buffer) return;
    const int localX = screenX - originX;
    const int localY = screenY - originY;
    if (localX < 0 || localX >= width || localY < bandStart || localY >= bandStart + bandRows) return;

    const int bandY = localY - bandStart;
    const int byteIdx = bandY * bytesPerRow + (localX >> 2);
    const int bitShift = 6 - (localX & 3) * 2;  // MSB first: pixel 0 at bits 6-7
    buffer[byteIdx] = (buffer[byteIdx] & ~(0x03 << bitShift)) | ((value & 0x03) << bitShift);
  }

  bool advanceTo(int newTopRow) {
    if (!ok) return false;
    if (newTopRow <= bandStart) return true;
    if (newTopRow > height) newTopRow = height;

    for (int row = bandStart; row < newTopRow; row++) {
      const int bandY = row - bandStart;
      const uint8_t* rowPtr =
          (bandY >= 0 && bandY < bandRows) ? buffer + static_cast<size_t>(bandY) * bytesPerRow : zeroRow;
      if (file.write(rowPtr, static_cast<size_t>(bytesPerRow)) != static_cast<size_t>(bytesPerRow)) {
        LOG_ERR("IMG", "Cache write error at row %d", row);
        ok = false;
        return false;
      }
    }

    flushedRows = newTopRow;
    bandStart = newTopRow;
    memset(buffer, 0, static_cast<size_t>(bandRows) * bytesPerRow);
    return true;
  }

  bool finalize() {
    if (!ok) {
      abort();
      return false;
    }

    for (int row = flushedRows; row < height; row++) {
      const int bandY = row - bandStart;
      const uint8_t* rowPtr =
          (bandY >= 0 && bandY < bandRows) ? buffer + static_cast<size_t>(bandY) * bytesPerRow : zeroRow;
      if (file.write(rowPtr, static_cast<size_t>(bytesPerRow)) != static_cast<size_t>(bytesPerRow)) {
        LOG_ERR("IMG", "Cache write error at row %d", row);
        abort();
        return false;
      }
    }

    file.close();
    LOG_DBG("IMG", "Cache written: %s (%dx%d, %d bytes)", cachePathStr.c_str(), width, height,
            4 + bytesPerRow * height);
    ok = false;
    return true;
  }

  void abort() {
    if (file.isOpen()) file.close();
    if (!cachePathStr.empty()) {
      Storage.remove(cachePathStr.c_str());
    }
    ok = false;
  }

  ~PixelCache() {
    if (file.isOpen()) {
      abort();
    }
    if (buffer) {
      freeBandBuffer(buffer);
      buffer = nullptr;
      zeroRow = nullptr;
    }
  }
};

#include "TxtReaderActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Serialization.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdint>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "ReaderQuickSettingsActivity.h"
#include "ReaderUtils.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ScreenshotUtil.h"

namespace {
constexpr size_t CHUNK_SIZE = 8 * 1024;  // 8KB chunk for reading
// Cache file magic and version
constexpr uint32_t CACHE_MAGIC = 0x54585449;  // "TXTI"
constexpr uint8_t CACHE_VERSION = 5;          // Increment when cache format or layout-affecting fields change
constexpr size_t CACHE_COMPLETE_OFFSET = sizeof(uint32_t) + sizeof(uint8_t);
constexpr size_t CACHE_PAGE_COUNT_OFFSET = CACHE_COMPLETE_OFFSET + sizeof(uint8_t) + sizeof(uint32_t) +
                                           sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) +
                                           sizeof(uint8_t);
constexpr size_t CACHE_HEADER_SIZE = CACHE_PAGE_COUNT_OFFSET + sizeof(uint32_t);
constexpr uint32_t MAX_PAGE_INDEX_ENTRIES = 200000;
constexpr EpubReaderMenuActivity::ActionMask TXT_READER_MENU_ACTIONS =
    EpubReaderMenuActivity::actionMask(EpubReaderMenuActivity::MenuAction::COLOR_MODE) |
    EpubReaderMenuActivity::actionMask(EpubReaderMenuActivity::MenuAction::SCREENSHOT) |
    EpubReaderMenuActivity::actionMask(EpubReaderMenuActivity::MenuAction::GO_HOME) |
    EpubReaderMenuActivity::actionMask(EpubReaderMenuActivity::MenuAction::DELETE_CACHE);

int clampPercent(int percent) {
  if (percent < 0) {
    return 0;
  }
  if (percent > 100) {
    return 100;
  }
  return percent;
}

size_t utf8CharBytesAt(const std::string& text, size_t pos) {
  if (pos >= text.size()) return 0;
  const auto lead = static_cast<uint8_t>(text[pos]);
  const size_t remaining = text.size() - pos;
  if (lead < 0x80) return 1;
  if ((lead >> 5) == 0x6 && remaining >= 2) return 2;
  if ((lead >> 4) == 0xE && remaining >= 3) return 3;
  if ((lead >> 3) == 0x1E && remaining >= 4) return 4;
  return 1;
}

size_t firstUtf8CharBytes(const std::string& text) { return utf8CharBytesAt(text, 0); }

size_t findFittingPrefixBytes(const GfxRenderer& renderer, const int fontId, const std::string& text,
                              const int maxWidth) {
  size_t pos = 0;
  size_t best = 0;
  size_t lastSpace = std::string::npos;
  uint8_t measureCount = 0;

  while (pos < text.size()) {
    const size_t charBytes = utf8CharBytesAt(text, pos);
    if (charBytes == 0) break;

    const size_t next = pos + charBytes;
    if (renderer.getTextWidth(fontId, text.substr(0, next).c_str()) > maxWidth) {
      break;
    }

    if (text[pos] == ' ') {
      lastSpace = pos;
    }
    best = next;
    pos = next;

    if ((++measureCount & 0x1F) == 0) {
      vTaskDelay(1);
    }
  }

  if (lastSpace != std::string::npos && lastSpace > 0 && lastSpace < best) {
    return lastSpace;
  }
  return best > 0 ? best : firstUtf8CharBytes(text);
}
}  // namespace

void TxtReaderActivity::onEnter() {
  Activity::onEnter();

  if (!txt) {
    return;
  }

  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
  mappedInput.setTouchOrientation(SETTINGS.orientation);

  txt->setupCacheDir();

  // Save current txt as last opened file and add to recent books
  auto filePath = txt->getPath();
  auto fileName = filePath.substr(filePath.rfind('/') + 1);
  APP_STATE.openEpubPath = filePath;
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(filePath, fileName, "", "");

  // Trigger first update
  requestUpdate();
}

void TxtReaderActivity::onExit() {
  Activity::onExit();

  // Reset orientation back to portrait for the rest of the UI
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  mappedInput.setTouchOrientation(CrossPointSettings::PORTRAIT);

  pageOffsets.clear();
  currentPageLines.clear();
  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  txt.reset();
}

void TxtReaderActivity::loop() {
  if (ReaderUtils::wasBackGesture(mappedInput)) {
    LOG_DBG("TRS", "reader gesture back to file browser");
    activityManager.goToFileBrowser(txt ? txt->getPath() : "");
    return;
  }

  if (readerBackOverlayVisible && ReaderUtils::wasBackOverlayTap(mappedInput)) {
    const auto action =
        ReaderUtils::hitTestReaderQuickBar(renderer, true, mappedInput.getTouchX(), mappedInput.getTouchY());
    readerBackOverlayVisible = false;
    if (action == ReaderUtils::QuickBarAction::Back) {
      LOG_DBG("TRS", "reader quick bar back");
      activityManager.goToFileBrowser(txt ? txt->getPath() : "");
      return;
    }
    if (action == ReaderUtils::QuickBarAction::Home) {
      LOG_DBG("TRS", "reader quick bar home");
      onGoHome();
      return;
    }
    if (action == ReaderUtils::QuickBarAction::Settings) {
      LOG_DBG("TRS", "reader quick bar settings");
      openReaderQuickSettings();
      return;
    }
    requestUpdate();
    return;
  }

  if (ReaderUtils::wasBackRevealGesture(mappedInput)) {
    LOG_DBG("TRS", "reader gesture show back overlay");
    readerBackOverlayVisible = true;
    requestUpdate();
    return;
  }

  if (ReaderUtils::wasMenuGesture(mappedInput)) {
    LOG_DBG("TRS", "reader gesture open quick settings");
    readerBackOverlayVisible = false;
    openReaderQuickSettings();
    return;
  }

  // Long press BACK (1s+) goes to file selection
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= ReaderUtils::GO_HOME_MS) {
    activityManager.goToFileBrowser(txt ? txt->getPath() : "");
    return;
  }

  const bool menuPressActive = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  if (!menuPressActive) {
    readerMenuLongPressHandled = false;
  } else if (!readerMenuLongPressHandled && mappedInput.getHeldTime() >= ReaderUtils::READER_MENU_LONG_PRESS_MS) {
    readerMenuLongPressHandled = true;
    readerBackOverlayVisible = false;
    openReaderMenu();
    return;
  }

  // Short press BACK goes directly to home
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) &&
      mappedInput.getHeldTime() < ReaderUtils::GO_HOME_MS) {
    onGoHome();
    return;
  }

  auto [prevTriggered, nextTriggered] = ReaderUtils::detectPageTurn(mappedInput);
  if (!prevTriggered && !nextTriggered) {
    return;
  }

  readerBackOverlayVisible = false;

  if (prevTriggered && currentPage > 0) {
    currentPage--;
    requestUpdate();
  } else if (nextTriggered) {
    if (ensurePageIndexed(currentPage + 1)) {
      currentPage++;
      requestUpdate();
    } else {
      onGoHome();
    }
  }
}

void TxtReaderActivity::openReaderMenu() {
  if (!txt) {
    return;
  }

  const int progressPercent =
      totalPages > 0 ? clampPercent(static_cast<int>((currentPage + 1) * 100L / totalPages)) : 0;
  startActivityForResult(
      std::make_unique<EpubReaderMenuActivity>(renderer, mappedInput, txt->getTitle(), currentPage + 1, totalPages,
                                               progressPercent, SETTINGS.orientation, false, TXT_READER_MENU_ACTIONS),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          onReaderMenuConfirm(static_cast<EpubReaderMenuActivity::MenuAction>(std::get<MenuResult>(result.data).action));
        } else {
          requestUpdate();
        }
      });
}

void TxtReaderActivity::openReaderQuickSettings() {
  startActivityForResult(std::make_unique<ReaderQuickSettingsActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             requestUpdate();
                             return;
                           }
                           ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
                           mappedInput.setTouchOrientation(SETTINGS.orientation);
                           initialized = false;
                           pageOffsets.clear();
                           currentPageLines.clear();
                           pagesUntilFullRefresh = 1;
                           requestUpdate();
                         });
}

void TxtReaderActivity::onReaderMenuConfirm(EpubReaderMenuActivity::MenuAction action) {
  switch (action) {
    case EpubReaderMenuActivity::MenuAction::GO_HOME:
      onGoHome();
      return;
    case EpubReaderMenuActivity::MenuAction::DELETE_CACHE: {
      if (txt) {
        const std::string indexPath = txt->getCachePath() + "/index.bin";
        Storage.remove(indexPath.c_str());
        saveProgress();
      }
      onGoHome();
      return;
    }
    case EpubReaderMenuActivity::MenuAction::SCREENSHOT:
      pendingScreenshot = true;
      requestUpdate();
      return;
    default:
      requestUpdate();
      return;
  }
}

void TxtReaderActivity::initializeReader() {
  if (initialized) {
    return;
  }

  // Store current settings for cache validation
  cachedFontId = SETTINGS.getReaderFontId();
  cachedScreenMargin = SETTINGS.screenMargin;
  cachedParagraphAlignment = SETTINGS.paragraphAlignment;

  // Calculate viewport dimensions
  renderer.getOrientedViewableTRBL(&cachedOrientedMarginTop, &cachedOrientedMarginRight, &cachedOrientedMarginBottom,
                                   &cachedOrientedMarginLeft);
  cachedOrientedMarginTop += cachedScreenMargin;
  cachedOrientedMarginLeft += cachedScreenMargin;
  cachedOrientedMarginRight += cachedScreenMargin;
  cachedOrientedMarginBottom +=
      std::max(cachedScreenMargin, static_cast<uint8_t>(UITheme::getInstance().getStatusBarHeight()));

  viewportWidth = renderer.getScreenWidth() - cachedOrientedMarginLeft - cachedOrientedMarginRight;
  const int viewportHeight = renderer.getScreenHeight() - cachedOrientedMarginTop - cachedOrientedMarginBottom;
  const int lineHeight = renderer.getLineHeight(cachedFontId);

  linesPerPage = viewportHeight / lineHeight;
  if (linesPerPage < 1) linesPerPage = 1;

  LOG_DBG("TRS", "Viewport: %dx%d, lines per page: %d", viewportWidth, viewportHeight, linesPerPage);

  // Try to load cached page index first. If no valid cache exists, seed a
  // one-page index and extend it lazily as the user reads.
  if (!loadPageIndexCache()) {
    resetPageIndex();
    savePageIndexCache();
  }

  // Load saved progress
  loadProgress();

  initialized = true;
}

void TxtReaderActivity::resetPageIndex() {
  pageOffsets.clear();
  pageIndexComplete = txt->getFileSize() == 0;

  if (!pageIndexComplete) {
    pageOffsets.reserve(64);
    pageOffsets.push_back(0);  // First page starts at offset 0
  }

  updateEstimatedTotalPages();
  LOG_DBG("TRS", "Seeded lazy page index for %zu bytes", txt->getFileSize());
}

bool TxtReaderActivity::ensurePageIndexed(const int targetPage) {
  if (targetPage < 0) {
    return false;
  }

  const size_t target = static_cast<size_t>(targetPage);
  const size_t fileSize = txt->getFileSize();

  while (pageOffsets.size() <= target && !pageIndexComplete) {
    if (pageOffsets.empty() || fileSize == 0) {
      pageIndexComplete = true;
      updateEstimatedTotalPages();
      updatePageIndexCacheState();
      break;
    }

    const size_t offset = pageOffsets.back();
    size_t nextOffset = offset;
    std::vector<std::string> tempLines;
    tempLines.reserve(linesPerPage);

    if (!loadPageAtOffset(offset, tempLines, nextOffset) || nextOffset <= offset || nextOffset >= fileSize) {
      pageIndexComplete = true;
      updateEstimatedTotalPages();
      updatePageIndexCacheState();
      break;
    }

    pageOffsets.push_back(nextOffset);
    updateEstimatedTotalPages();
    appendPageIndexCacheOffset(nextOffset);
    vTaskDelay(1);
  }

  return target < pageOffsets.size();
}

void TxtReaderActivity::updateEstimatedTotalPages() {
  if (pageOffsets.empty()) {
    totalPages = 0;
    return;
  }

  if (pageIndexComplete || pageOffsets.size() == 1) {
    totalPages = static_cast<int>(pageOffsets.size());
    return;
  }

  const size_t indexedSpan = pageOffsets.back() - pageOffsets.front();
  const size_t indexedIntervals = pageOffsets.size() - 1;
  const size_t avgPageBytes = indexedIntervals > 0 ? indexedSpan / indexedIntervals : 0;

  if (avgPageBytes == 0) {
    totalPages = static_cast<int>(pageOffsets.size());
    return;
  }

  const size_t estimatedPages = (txt->getFileSize() + avgPageBytes - 1) / avgPageBytes;
  totalPages = static_cast<int>(std::max(estimatedPages, pageOffsets.size()));
}

bool TxtReaderActivity::loadPageAtOffset(size_t offset, std::vector<std::string>& outLines, size_t& nextOffset) {
  outLines.clear();
  outLines.reserve(linesPerPage);
  const size_t fileSize = txt->getFileSize();

  if (offset >= fileSize) {
    return false;
  }

  // Read a chunk from file
  size_t chunkSize = std::min(CHUNK_SIZE, fileSize - offset);
  auto* buffer = static_cast<uint8_t*>(malloc(chunkSize + 1));
  if (!buffer) {
    LOG_ERR("TRS", "Failed to allocate %zu bytes", chunkSize);
    return false;
  }

  if (!txt->readContent(buffer, offset, chunkSize)) {
    free(buffer);
    return false;
  }
  buffer[chunkSize] = '\0';

  // Parse lines from buffer
  size_t pos = 0;

  while (pos < chunkSize && static_cast<int>(outLines.size()) < linesPerPage) {
    // Find end of line
    size_t lineEnd = pos;
    while (lineEnd < chunkSize && buffer[lineEnd] != '\n') {
      lineEnd++;
    }

    // Check if we have a complete line
    bool lineComplete = (lineEnd < chunkSize) || (offset + lineEnd >= fileSize);

    if (!lineComplete && static_cast<int>(outLines.size()) > 0) {
      // Incomplete line and we already have some lines, stop here
      break;
    }

    // Calculate the actual length of line content in the buffer (excluding newline)
    size_t lineContentLen = lineEnd - pos;

    // Check for carriage return
    bool hasCR = (lineContentLen > 0 && buffer[pos + lineContentLen - 1] == '\r');
    size_t displayLen = hasCR ? lineContentLen - 1 : lineContentLen;

    const TxtEncoding encoding = txt->getEncoding();
    const bool skipBom = encoding == TxtEncoding::Utf8Bom && offset + pos == 0 && displayLen >= 3 &&
                         buffer[pos] == 0xEF && buffer[pos + 1] == 0xBB && buffer[pos + 2] == 0xBF;
    const size_t sourcePrefixSkip = skipBom ? 3 : 0;
    const uint8_t* const sourceLine = buffer + pos + sourcePrefixSkip;
    const size_t displaySourceLen = displayLen - sourcePrefixSkip;
    const TxtEncoding lineEncoding = encoding == TxtEncoding::Utf8Bom ? TxtEncoding::Utf8 : encoding;

    // Extract line content for display (without CR/LF), converting legacy GBK files to UTF-8.
    std::string line = txtBytesToUtf8(sourceLine, displaySourceLen, lineEncoding);

    // Track position within this source line (in bytes from pos)
    size_t lineSourceBytePos = sourcePrefixSkip;

    // Word wrap if needed
    while (!line.empty() && static_cast<int>(outLines.size()) < linesPerPage) {
      int lineWidth = renderer.getTextWidth(cachedFontId, line.c_str());

      if (lineWidth <= viewportWidth) {
        outLines.push_back(line);
        lineSourceBytePos = displayLen;  // Consumed entire source line content
        line.clear();
        break;
      }

      size_t breakPos = findFittingPrefixBytes(renderer, cachedFontId, line, viewportWidth);

      if (breakPos == 0) {
        breakPos = firstUtf8CharBytes(line);
      }

      outLines.push_back(line.substr(0, breakPos));

      // Skip space at break point
      size_t skipUtf8Bytes = breakPos;
      const size_t lineRelativeSourcePos = lineSourceBytePos - sourcePrefixSkip;
      size_t sourceBytes = txtSourceBytesForUtf8Prefix(sourceLine + lineRelativeSourcePos,
                                                       displaySourceLen - lineRelativeSourcePos, lineEncoding, breakPos);
      if (breakPos < line.length() && line[breakPos] == ' ') {
        skipUtf8Bytes++;
        if (lineRelativeSourcePos + sourceBytes < displaySourceLen) {
          sourceBytes++;
        }
      }
      if (sourceBytes == 0) {
        sourceBytes = 1;
      }
      lineSourceBytePos += sourceBytes;
      line = line.substr(skipUtf8Bytes);
    }

    // Determine how much of the source buffer we consumed
    if (line.empty()) {
      // Fully consumed this source line, move past the newline
      pos = lineEnd + 1;
    } else {
      // Partially consumed - page is full mid-line
      // Move pos to where we stopped in the line (NOT past the line)
      pos = pos + lineSourceBytePos;
      break;
    }
  }

  // Ensure we make progress even if calculations go wrong
  if (pos == 0 && !outLines.empty()) {
    // Fallback: at minimum, consume something to avoid infinite loop
    pos = 1;
  }

  nextOffset = offset + pos;

  // Make sure we don't go past the file
  if (nextOffset > fileSize) {
    nextOffset = fileSize;
  }

  free(buffer);

  return !outLines.empty();
}

void TxtReaderActivity::render(RenderLock&&) {
  if (!txt) {
    return;
  }

  // Initialize reader if not done
  if (!initialized) {
    initializeReader();
  }

  if (pageOffsets.empty()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_EMPTY_FILE), true, EpdFontFamily::BOLD);
    ReaderUtils::drawReaderBackOverlay(renderer, readerBackOverlayVisible);
    renderer.displayBuffer();
    return;
  }

  // Bounds check
  if (currentPage < 0) currentPage = 0;
  if (!ensurePageIndexed(currentPage)) {
    currentPage = static_cast<int>(pageOffsets.size()) - 1;
  }
  if (currentPage < 0) currentPage = 0;

  // Load current page content
  size_t offset = pageOffsets[currentPage];
  size_t nextOffset;
  currentPageLines.clear();
  loadPageAtOffset(offset, currentPageLines, nextOffset);

  renderer.clearScreen();
  renderPage();

  // Save progress
  saveProgress();

  if (pendingScreenshot) {
    pendingScreenshot = false;
    ScreenshotUtil::takeScreenshot(renderer);
  }
}

void TxtReaderActivity::renderPage() {
  const int lineHeight = renderer.getLineHeight(cachedFontId);
  const int contentWidth = viewportWidth;

  // Render text lines with alignment
  auto renderLines = [&]() {
    int y = cachedOrientedMarginTop;
    for (const auto& line : currentPageLines) {
      if (!line.empty()) {
        int x = cachedOrientedMarginLeft;

        // Apply text alignment
        switch (cachedParagraphAlignment) {
          case CrossPointSettings::LEFT_ALIGN:
          default:
            // x already set to left margin
            break;
          case CrossPointSettings::CENTER_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = cachedOrientedMarginLeft + (contentWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = cachedOrientedMarginLeft + contentWidth - textWidth;
            break;
          }
          case CrossPointSettings::JUSTIFIED:
            // For plain text, justified is treated as left-aligned
            // (true justification would require word spacing adjustments)
            break;
        }

        renderer.drawText(cachedFontId, x, y, line.c_str());
      }
      y += lineHeight;
    }
  };

  // Font prewarm: scan pass accumulates text, then prewarm, then real render
  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  renderLines();  // scan pass — text accumulated, no drawing
  scope.endScanAndPrewarm();

  if (SETTINGS.textAntiAliasing) {
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_DIRECT);
  }

  renderLines();
  renderer.setRenderMode(GfxRenderer::BW);
  renderStatusBar();
  ReaderUtils::drawReaderBackOverlay(renderer, readerBackOverlayVisible);

  ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);
  // scope destructor clears font cache via FontCacheManager
}

void TxtReaderActivity::renderStatusBar() const {
  const float progress = totalPages > 0 ? (currentPage + 1) * 100.0f / totalPages : 0;
  std::string title;
  if (SETTINGS.statusBarTitle != CrossPointSettings::STATUS_BAR_TITLE::HIDE_TITLE) {
    title = txt->getTitle();
  }
  GUI.drawStatusBar(renderer, progress, currentPage + 1, totalPages, title);
}

void TxtReaderActivity::saveProgress() const {
  FsFile f;
  if (Storage.openFileForWrite("TRS", txt->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    data[0] = currentPage & 0xFF;
    data[1] = (currentPage >> 8) & 0xFF;
    data[2] = (currentPage >> 16) & 0xFF;
    data[3] = (currentPage >> 24) & 0xFF;
    f.write(data, 4);
    f.close();
  }
}

void TxtReaderActivity::loadProgress() {
  FsFile f;
  if (Storage.openFileForRead("TRS", txt->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    if (f.read(data, 4) == 4) {
      currentPage = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
      const int indexedPages = static_cast<int>(pageOffsets.size());
      if (pageIndexComplete && currentPage >= indexedPages) {
        currentPage = indexedPages - 1;
      } else if (!pageIndexComplete && currentPage >= indexedPages) {
        currentPage = indexedPages > 0 ? indexedPages - 1 : 0;
      }
      if (currentPage < 0) {
        currentPage = 0;
      }
      LOG_DBG("TRS", "Loaded progress: page %d/%d", currentPage, totalPages);
    }
    f.close();
  }
}

bool TxtReaderActivity::loadPageIndexCache() {
  // Cache file format (using serialization module):
  // - uint32_t: magic "TXTI"
  // - uint8_t: cache version
  // - uint8_t: complete index flag
  // - uint32_t: file size (to validate cache)
  // - int32_t: viewport width
  // - int32_t: lines per page
  // - int32_t: font ID (to invalidate cache on font change)
  // - int32_t: screen margin (to invalidate cache on margin change)
  // - uint8_t: paragraph alignment (to invalidate cache on alignment change)
  // - uint32_t: total pages count
  // - N * uint32_t: page offsets

  std::string cachePath = txt->getCachePath() + "/index.bin";
  FsFile f;
  if (!Storage.openFileForRead("TRS", cachePath, f)) {
    LOG_DBG("TRS", "No page index cache found");
    return false;
  }

  // Read and validate header using serialization module
  uint32_t magic;
  serialization::readPod(f, magic);
  if (magic != CACHE_MAGIC) {
    LOG_DBG("TRS", "Cache magic mismatch, rebuilding");
    f.close();
    return false;
  }

  uint8_t version;
  serialization::readPod(f, version);
  if (version != CACHE_VERSION) {
    LOG_DBG("TRS", "Cache version mismatch (%d != %d), rebuilding", version, CACHE_VERSION);
    f.close();
    return false;
  }

  uint8_t completeFlag;
  serialization::readPod(f, completeFlag);

  uint32_t fileSize;
  serialization::readPod(f, fileSize);
  if (fileSize != txt->getFileSize()) {
    LOG_DBG("TRS", "Cache file size mismatch, rebuilding");
    f.close();
    return false;
  }

  int32_t cachedWidth;
  serialization::readPod(f, cachedWidth);
  if (cachedWidth != viewportWidth) {
    LOG_DBG("TRS", "Cache viewport width mismatch, rebuilding");
    f.close();
    return false;
  }

  int32_t cachedLines;
  serialization::readPod(f, cachedLines);
  if (cachedLines != linesPerPage) {
    LOG_DBG("TRS", "Cache lines per page mismatch, rebuilding");
    f.close();
    return false;
  }

  int32_t fontId;
  serialization::readPod(f, fontId);
  if (fontId != cachedFontId) {
    LOG_DBG("TRS", "Cache font ID mismatch (%d != %d), rebuilding", fontId, cachedFontId);
    f.close();
    return false;
  }

  int32_t margin;
  serialization::readPod(f, margin);
  if (margin != cachedScreenMargin) {
    LOG_DBG("TRS", "Cache screen margin mismatch, rebuilding");
    f.close();
    return false;
  }

  uint8_t alignment;
  serialization::readPod(f, alignment);
  if (alignment != cachedParagraphAlignment) {
    LOG_DBG("TRS", "Cache paragraph alignment mismatch, rebuilding");
    f.close();
    return false;
  }

  uint32_t numPages;
  serialization::readPod(f, numPages);
  if ((numPages == 0 && txt->getFileSize() > 0) || numPages > MAX_PAGE_INDEX_ENTRIES ||
      numPages > txt->getFileSize() + 1 ||
      f.fileSize() < CACHE_HEADER_SIZE + numPages * sizeof(uint32_t)) {
    LOG_DBG("TRS", "Cache page count invalid, rebuilding");
    f.close();
    return false;
  }

  // Read page offsets
  pageOffsets.clear();
  pageOffsets.reserve(numPages);

  for (uint32_t i = 0; i < numPages; i++) {
    uint32_t offset;
    serialization::readPod(f, offset);
    if (offset > txt->getFileSize() || (i > 0 && offset <= pageOffsets.back())) {
      LOG_DBG("TRS", "Cache offset invalid, rebuilding");
      f.close();
      return false;
    }
    pageOffsets.push_back(offset);
  }

  pageIndexComplete = completeFlag != 0;
  updateEstimatedTotalPages();
  f.close();
  LOG_DBG("TRS", "Loaded page index cache: %d pages (%s)", static_cast<int>(pageOffsets.size()),
          pageIndexComplete ? "complete" : "partial");
  return true;
}

void TxtReaderActivity::savePageIndexCache() const {
  std::string cachePath = txt->getCachePath() + "/index.bin";
  FsFile f;
  if (!Storage.openFileForWrite("TRS", cachePath, f)) {
    LOG_ERR("TRS", "Failed to save page index cache");
    return;
  }

  // Write header using serialization module
  serialization::writePod(f, CACHE_MAGIC);
  serialization::writePod(f, CACHE_VERSION);
  serialization::writePod(f, static_cast<uint8_t>(pageIndexComplete ? 1 : 0));
  serialization::writePod(f, static_cast<uint32_t>(txt->getFileSize()));
  serialization::writePod(f, static_cast<int32_t>(viewportWidth));
  serialization::writePod(f, static_cast<int32_t>(linesPerPage));
  serialization::writePod(f, static_cast<int32_t>(cachedFontId));
  serialization::writePod(f, static_cast<int32_t>(cachedScreenMargin));
  serialization::writePod(f, cachedParagraphAlignment);
  serialization::writePod(f, static_cast<uint32_t>(pageOffsets.size()));

  // Write page offsets
  for (size_t offset : pageOffsets) {
    serialization::writePod(f, static_cast<uint32_t>(offset));
  }
  f.close();

  LOG_DBG("TRS", "Saved page index cache: %d pages", totalPages);
}

bool TxtReaderActivity::appendPageIndexCacheOffset(const size_t offset) const {
  const std::string cachePath = txt->getCachePath() + "/index.bin";
  FsFile f = Storage.open(cachePath.c_str(), O_RDWR | O_CREAT);
  if (!f) {
    LOG_ERR("TRS", "Failed to append page index cache");
    return false;
  }

  if (f.fileSize() < CACHE_HEADER_SIZE || !f.seek(f.fileSize())) {
    f.close();
    savePageIndexCache();
    return false;
  }

  serialization::writePod(f, static_cast<uint32_t>(offset));
  f.seek(CACHE_PAGE_COUNT_OFFSET);
  serialization::writePod(f, static_cast<uint32_t>(pageOffsets.size()));
  f.seek(CACHE_COMPLETE_OFFSET);
  serialization::writePod(f, static_cast<uint8_t>(pageIndexComplete ? 1 : 0));
  f.close();
  return true;
}

bool TxtReaderActivity::updatePageIndexCacheState() const {
  const std::string cachePath = txt->getCachePath() + "/index.bin";
  FsFile f = Storage.open(cachePath.c_str(), O_RDWR);
  if (!f) {
    savePageIndexCache();
    return false;
  }

  if (!f.seek(CACHE_COMPLETE_OFFSET)) {
    f.close();
    savePageIndexCache();
    return false;
  }

  serialization::writePod(f, static_cast<uint8_t>(pageIndexComplete ? 1 : 0));
  f.seek(CACHE_PAGE_COUNT_OFFSET);
  serialization::writePod(f, static_cast<uint32_t>(pageOffsets.size()));
  f.close();
  return true;
}

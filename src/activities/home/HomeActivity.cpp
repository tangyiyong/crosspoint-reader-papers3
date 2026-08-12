#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Utf8.h>
#include <Xtc.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int HOME_TODAY_HISTORY_HEIGHT = 160;
constexpr int HOME_TODAY_HISTORY_TOP_MARGIN = 6;
constexpr int HOME_MENU_VERTICAL_PADDING = 5;
constexpr int HOME_MENU_ROW_HEIGHT = 42;
constexpr int HOME_MENU_SPACING = 4;
}  // namespace

int HomeActivity::getMenuItemCount() const {
  int count = 5;  // App Suite, File Browser, Recents, File transfer, Settings
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsUrl) {
    count++;
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (!Storage.exists(book.path.c_str())) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(coverHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(coverHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
#if CROSSPOINT_PAPERS3
  if (showingLoading) {
    renderer.requestFullRefresh();
  }
#endif
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  hasOpdsUrl = OPDS_STORE.hasServers();

  selectorIndex = 0;

  const auto& metrics = UITheme::getInstance().getMetrics();
  loadRecentBooks(metrics.homeRecentBooksCount);
  loadTodayHistoryCache();

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  uint8_t* frameBuffer = renderer.getFrameBuffer();
  if (!frameBuffer) {
    return false;
  }

  // Free any existing buffer first
  freeCoverBuffer();

  const size_t bufferSize = GfxRenderer::getBufferSize();
  coverBuffer = static_cast<uint8_t*>(malloc(bufferSize));
  if (!coverBuffer) {
    return false;
  }

  memcpy(coverBuffer, frameBuffer, bufferSize);
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer) {
    return false;
  }

  uint8_t* frameBuffer = renderer.getFrameBuffer();
  if (!frameBuffer) {
    return false;
  }

  const size_t bufferSize = GfxRenderer::getBufferSize();
  memcpy(frameBuffer, coverBuffer, bufferSize);
  return true;
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferStored = false;
}

void HomeActivity::loop() {
  const int menuCount = getMenuItemCount();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  if (!firstRenderDone) {
    return;
  }

  if (!todayHistorySyncAttempted) {
    syncTodayHistoryIfNeeded();
    return;
  }

  const Rect historyRect = todayHistoryRect(pageWidth, pageHeight);
  if (mappedInput.wasContentSwipedUp() || mappedInput.wasContentSwipedDown()) {
    const int maxTop = std::max(0, static_cast<int>(todayHistory.events.size()) - todayHistoryVisibleItems(historyRect));
    if (mappedInput.wasContentSwipedUp()) {
      todayHistoryTopIndex = std::min(maxTop, todayHistoryTopIndex + todayHistoryVisibleItems(historyRect));
    } else {
      todayHistoryTopIndex = std::max(0, todayHistoryTopIndex - todayHistoryVisibleItems(historyRect));
    }
    requestUpdate();
    return;
  }

  if (mappedInput.wasContentTapped()) {
    const int touchX = mappedInput.getTouchX();
    const int touchY = mappedInput.getTouchY();
    const Rect coverRect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight};
    if (!recentBooks.empty() && touchX >= coverRect.x && touchX < coverRect.x + coverRect.width &&
        touchY >= coverRect.y && touchY < coverRect.y + coverRect.height) {
      if (recentBooks.size() == 1) {
        selectorIndex = 0;
        onSelectBook(recentBooks[selectorIndex].path);
        return;
      }
    }

    const Rect menuRect = homeMenuRect(pageWidth, pageHeight);
    const int tappedMenuIndex =
        hitTestHomeMenu(menuRect, menuCount - static_cast<int>(recentBooks.size()), mappedInput.getTouchX(),
                        mappedInput.getTouchY());
    if (tappedMenuIndex >= 0) {
      selectorIndex = static_cast<int>(recentBooks.size()) + tappedMenuIndex;
      activateSelectedItem();
      return;
    }
  }

  buttonNavigator.onNext([this, menuCount] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuCount] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelectedItem();
  }
}

int HomeActivity::hitTestHomeMenu(const Rect rect, const int itemCount, const int touchX, const int touchY) const {
  if (touchX < rect.x || touchX >= rect.x + rect.width || touchY < rect.y || touchY >= rect.y + rect.height) {
    return -1;
  }
  for (int i = 0; i < itemCount; ++i) {
    const int y = rect.y + i * (HOME_MENU_ROW_HEIGHT + HOME_MENU_SPACING);
    if (touchY >= y && touchY < y + HOME_MENU_ROW_HEIGHT) {
      return i;
    }
  }
  return -1;
}

void HomeActivity::drawCompactHomeMenu(const Rect rect, const std::vector<const char*>& menuItems) {
  const int selectedMenuIndex = selectorIndex - static_cast<int>(recentBooks.size());
  const int rowW = rect.width - UITheme::getInstance().getMetrics().contentSidePadding * 2;
  const int rowX = rect.x + UITheme::getInstance().getMetrics().contentSidePadding;
  for (int i = 0; i < static_cast<int>(menuItems.size()); ++i) {
    const int rowY = rect.y + i * (HOME_MENU_ROW_HEIGHT + HOME_MENU_SPACING);
    if (rowY + HOME_MENU_ROW_HEIGHT > rect.y + rect.height) {
      break;
    }
    const bool selected = selectedMenuIndex == i;
    if (selected) {
      renderer.fillRect(rowX, rowY, rowW, HOME_MENU_ROW_HEIGHT);
    } else {
      renderer.drawRect(rowX, rowY, rowW, HOME_MENU_ROW_HEIGHT);
    }
    const std::string label = renderer.truncatedText(UI_10_FONT_ID, menuItems[i], rowW - 24);
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, label.c_str());
    const int textY = rowY + (HOME_MENU_ROW_HEIGHT - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawText(UI_10_FONT_ID, rowX + (rowW - textWidth) / 2, textY, label.c_str(), !selected);
  }
}

void HomeActivity::loadTodayHistoryCache() {
  TodayHistoryClient::loadCached(todayHistory);
  todayHistoryTopIndex = 0;
  todayHistorySyncAttempted = false;
}

void HomeActivity::syncTodayHistoryIfNeeded() {
  todayHistorySyncAttempted = true;
  if (TodayHistoryClient::syncToday(true)) {
    TodayHistoryClient::loadCached(todayHistory);
    todayHistoryTopIndex = 0;
    requestUpdate();
  }
}

Rect HomeActivity::todayHistoryRect(const int pageWidth, const int pageHeight) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  return Rect{metrics.contentSidePadding, bottom - HOME_TODAY_HISTORY_HEIGHT, pageWidth - metrics.contentSidePadding * 2,
              HOME_TODAY_HISTORY_HEIGHT};
}

Rect HomeActivity::homeMenuRect(const int pageWidth, const int pageHeight) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect historyRect = todayHistoryRect(pageWidth, pageHeight);
  const int menuTop = metrics.homeTopPadding + metrics.homeCoverTileHeight + HOME_MENU_VERTICAL_PADDING;
  return Rect{0, menuTop, pageWidth, historyRect.y - menuTop - HOME_TODAY_HISTORY_TOP_MARGIN};
}

int HomeActivity::todayHistoryVisibleItems(const Rect rect) const {
  const int itemH = renderer.getLineHeight(UI_10_FONT_ID) + renderer.getLineHeight(SMALL_FONT_ID) + 7;
  return std::max(1, (rect.height - 34) / std::max(1, itemH));
}

void HomeActivity::drawTodayHistory(const Rect rect) const {
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height, true);
  const int titleY = rect.y + 8;
  renderer.drawText(SMALL_FONT_ID, rect.x + 10, titleY, tr(STR_TODAY_HISTORY), true, EpdFontFamily::BOLD);

  if (todayHistory.update[0] != '\0') {
    const int updateWidth = renderer.getTextWidth(SMALL_FONT_ID, todayHistory.update);
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - 10 - updateWidth, titleY, todayHistory.update);
  }

  if (!todayHistory.hasAny) {
    renderer.drawText(UI_10_FONT_ID, rect.x + 10, titleY + renderer.getLineHeight(SMALL_FONT_ID) + 10,
                      tr(STR_TODAY_HISTORY_EMPTY));
    return;
  }

  const int listTop = titleY + renderer.getLineHeight(SMALL_FONT_ID) + 8;
  const int maxWidth = rect.width - 20;
  const int itemH = renderer.getLineHeight(UI_10_FONT_ID) + renderer.getLineHeight(SMALL_FONT_ID) + 7;
  const int visible = todayHistoryVisibleItems(rect);
  const int end = std::min(static_cast<int>(todayHistory.events.size()), todayHistoryTopIndex + visible);
  int y = listTop;
  for (int i = todayHistoryTopIndex; i < end; ++i) {
    char line[96];
    snprintf(line, sizeof(line), "%s  %s", todayHistory.events[i].year, todayHistory.events[i].title.c_str());
    const std::string title = renderer.truncatedText(UI_10_FONT_ID, line, maxWidth, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, rect.x + 10, y, title.c_str(), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 2;

    const std::string desc =
        renderer.truncatedText(SMALL_FONT_ID, todayHistory.events[i].desc.c_str(), maxWidth, EpdFontFamily::REGULAR);
    renderer.drawText(SMALL_FONT_ID, rect.x + 10, y, desc.c_str());
    y += renderer.getLineHeight(SMALL_FONT_ID) + 5;
  }

  if (todayHistory.events.size() > static_cast<size_t>(visible)) {
    const int scrollX = rect.x + rect.width - 5;
    const int scrollTop = listTop;
    const int scrollH = rect.y + rect.height - listTop - 6;
    const int maxTop = std::max(1, static_cast<int>(todayHistory.events.size()) - visible);
    const int thumbH = std::max(12, scrollH * visible / static_cast<int>(todayHistory.events.size()));
    const int thumbY = scrollTop + (scrollH - thumbH) * todayHistoryTopIndex / maxTop;
    renderer.drawLine(scrollX, scrollTop, scrollX, scrollTop + scrollH, true);
    renderer.fillRect(scrollX - 2, thumbY, 3, thumbH, true);
  }
}

void HomeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  bool bufferRestored = coverBufferStored && restoreCoverBuffer();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding}, nullptr);

  GUI.drawRecentBookCover(renderer, Rect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight},
                          recentBooks, selectorIndex, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));

  // Build menu items dynamically
  std::vector<const char*> menuItems = {tr(STR_APP_SUITE), tr(STR_BROWSE_FILES), tr(STR_MENU_RECENT_BOOKS),
                                        tr(STR_FILE_TRANSFER), tr(STR_SETTINGS_TITLE)};
  if (hasOpdsUrl) {
    // Insert OPDS Browser after File Browser
    menuItems.insert(menuItems.begin() + 2, tr(STR_OPDS_BROWSER));
  }

  const Rect historyRect = todayHistoryRect(pageWidth, pageHeight);
  const Rect menuRect = homeMenuRect(pageWidth, pageHeight);
  drawCompactHomeMenu(menuRect, menuItems);

  drawTodayHistory(historyRect);

  const auto labels = mappedInput.mapLabels("", tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(metrics.homeCoverHeight);
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onRecentsOpen() { activityManager.goToRecentBooks(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onAppSuiteOpen() { activityManager.goToAppSuite(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }

void HomeActivity::activateSelectedItem() {
  int idx = 0;
  const int menuSelectedIndex = selectorIndex - static_cast<int>(recentBooks.size());
  const int appSuiteIdx = idx++;
  const int fileBrowserIdx = idx++;
  const int recentsIdx = idx++;
  const int opdsLibraryIdx = hasOpdsUrl ? idx++ : -1;
  const int fileTransferIdx = idx++;
  const int settingsIdx = idx;

  if (selectorIndex < static_cast<int>(recentBooks.size())) {
    onSelectBook(recentBooks[selectorIndex].path);
  } else if (menuSelectedIndex == appSuiteIdx) {
    onAppSuiteOpen();
  } else if (menuSelectedIndex == fileBrowserIdx) {
    onFileBrowserOpen();
  } else if (menuSelectedIndex == recentsIdx) {
    onRecentsOpen();
  } else if (menuSelectedIndex == opdsLibraryIdx) {
    onOpdsBrowserOpen();
  } else if (menuSelectedIndex == fileTransferIdx) {
    onFileTransferOpen();
  } else if (menuSelectedIndex == settingsIdx) {
    onSettingsOpen();
  }
}

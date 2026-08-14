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
constexpr int HOME_QUOTE_HEIGHT = 112;
constexpr int HOME_QUOTE_TOP_MARGIN = 14;
constexpr int HOME_MENU_VERTICAL_PADDING = 5;
constexpr int HOME_MENU_ROW_HEIGHT = 46;
constexpr int HOME_MENU_SPACING = 8;

int homeMenuHeight(const int itemCount) {
  return itemCount * HOME_MENU_ROW_HEIGHT + std::max(0, itemCount - 1) * HOME_MENU_SPACING;
}
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
  loadQuoteCache();

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

  if (!quoteSyncAttempted) {
    syncQuoteIfNeeded();
    return;
  }

  if (mappedInput.wasContentSwipedUp() || mappedInput.wasContentSwipedDown()) {
    return;
  }

  if (mappedInput.wasContentTapped()) {
    const int touchX = mappedInput.getTouchX();
    const int touchY = mappedInput.getTouchY();
    const Rect coverRect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight};
    const int tappedBookIndex = hitTestRecentBook(coverRect, touchX, touchY);
    if (tappedBookIndex >= 0) {
      selectorIndex = tappedBookIndex;
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }

    const Rect menuRect = homeMenuRect(pageWidth, pageHeight, menuCount - static_cast<int>(recentBooks.size()));
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

int HomeActivity::hitTestRecentBook(const Rect rect, const int touchX, const int touchY) const {
  if (recentBooks.empty() || touchX < rect.x || touchX >= rect.x + rect.width || touchY < rect.y ||
      touchY >= rect.y + rect.height) {
    return -1;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int visibleCount = std::min(static_cast<int>(recentBooks.size()), metrics.homeRecentBooksCount);
  if (visibleCount <= 0) {
    return -1;
  }

  const int tileAreaX = rect.x + metrics.contentSidePadding;
  const int tileAreaW = rect.width - metrics.contentSidePadding * 2;
  if (touchX < tileAreaX || touchX >= tileAreaX + tileAreaW) {
    return -1;
  }

  const int tileW = tileAreaW / visibleCount;
  if (tileW <= 0) {
    return -1;
  }
  const int index = std::min((touchX - tileAreaX) / tileW, visibleCount - 1);
  return index < static_cast<int>(recentBooks.size()) ? index : -1;
}

int HomeActivity::hitTestHomeMenu(const Rect rect, const int itemCount, const int touchX, const int touchY) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowX = rect.x + metrics.contentSidePadding;
  const int rowW = rect.width - metrics.contentSidePadding * 2;
  if (touchX < rowX || touchX >= rowX + rowW || touchY < rect.y || touchY >= rect.y + rect.height) {
    return -1;
  }
  for (int i = 0; i < itemCount; ++i) {
    const int rowY = rect.y + i * (HOME_MENU_ROW_HEIGHT + HOME_MENU_SPACING);
    if (touchY >= rowY && touchY < rowY + HOME_MENU_ROW_HEIGHT) {
      return i;
    }
  }
  return -1;
}

void HomeActivity::drawHomeMenu(const Rect rect, const std::vector<const char*>& menuItems) {
  const int selectedMenuIndex = selectorIndex - static_cast<int>(recentBooks.size());
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowW = rect.width - metrics.contentSidePadding * 2;
  const int rowX = rect.x + metrics.contentSidePadding;

  auto iconFor = [this](int index) {
    int i = 0;
    const int appSuiteIdx = i++;
    const int fileBrowserIdx = i++;
    const int recentsIdx = i++;
    const int opdsLibraryIdx = hasOpdsUrl ? i++ : -1;
    const int fileTransferIdx = i++;
    const int settingsIdx = i;
    if (index == appSuiteIdx) return UIIcon::Library;
    if (index == fileBrowserIdx) return UIIcon::Folder;
    if (index == recentsIdx) return UIIcon::Recent;
    if (index == opdsLibraryIdx) return UIIcon::Wifi;
    if (index == fileTransferIdx) return UIIcon::Transfer;
    if (index == settingsIdx) return UIIcon::Settings;
    return UIIcon::File;
  };

  auto drawIcon = [this](const UIIcon icon, const int x, const int y, const bool color) {
    switch (icon) {
      case UIIcon::Folder:
        renderer.drawRect(x, y + 7, 18, 13, color);
        renderer.drawLine(x, y + 7, x + 6, y + 3, color);
        renderer.drawLine(x + 6, y + 3, x + 13, y + 7, color);
        break;
      case UIIcon::Recent:
      case UIIcon::Book:
        renderer.drawRect(x, y + 3, 9, 18, color);
        renderer.drawRect(x + 9, y + 3, 9, 18, color);
        renderer.drawLine(x + 9, y + 5, x + 9, y + 20, color);
        break;
      case UIIcon::Wifi:
        renderer.drawLine(x + 2, y + 15, x + 9, y + 8, color);
        renderer.drawLine(x + 9, y + 8, x + 16, y + 15, color);
        renderer.drawLine(x + 5, y + 18, x + 9, y + 14, color);
        renderer.drawLine(x + 9, y + 14, x + 13, y + 18, color);
        renderer.fillRect(x + 8, y + 20, 3, 3, color);
        break;
      case UIIcon::Transfer:
        renderer.drawRect(x + 3, y + 3, 14, 19, color);
        renderer.drawLine(x + 10, y + 6, x + 10, y + 18, color);
        renderer.drawLine(x + 10, y + 6, x + 6, y + 10, color);
        renderer.drawLine(x + 10, y + 6, x + 14, y + 10, color);
        break;
      case UIIcon::Settings:
        renderer.drawRect(x + 7, y + 4, 6, 17, color);
        renderer.drawRect(x + 2, y + 9, 16, 7, color);
        break;
      case UIIcon::Library:
      default:
        renderer.drawRect(x + 2, y + 3, 16, 18, color);
        renderer.drawLine(x + 5, y + 8, x + 15, y + 8, color);
        renderer.drawLine(x + 5, y + 13, x + 15, y + 13, color);
        renderer.drawLine(x + 5, y + 18, x + 12, y + 18, color);
        break;
    }
  };

  for (int i = 0; i < static_cast<int>(menuItems.size()); ++i) {
    const int rowY = rect.y + i * (HOME_MENU_ROW_HEIGHT + HOME_MENU_SPACING);
    if (rowY + HOME_MENU_ROW_HEIGHT > rect.y + rect.height) {
      break;
    }
    const bool selected = selectedMenuIndex == i;
    renderer.fillRect(rowX, rowY, rowW, HOME_MENU_ROW_HEIGHT, selected);
    renderer.drawRect(rowX, rowY, rowW, HOME_MENU_ROW_HEIGHT, !selected);

    const int iconX = rowX + 12;
    const int iconY = rowY + (HOME_MENU_ROW_HEIGHT - 24) / 2;
    drawIcon(iconFor(i), iconX, iconY, !selected);

    const std::string label = renderer.truncatedText(UI_10_FONT_ID, menuItems[i], rowW - 56);
    const int textY = rowY + (HOME_MENU_ROW_HEIGHT - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawText(UI_10_FONT_ID, rowX + 44, textY, label.c_str(), !selected);
  }
}

void HomeActivity::loadQuoteCache() {
  QuoteDataClient::loadCached(homeQuote);
  quoteSyncAttempted = false;
}

void HomeActivity::syncQuoteIfNeeded() {
  quoteSyncAttempted = true;
  if (QuoteDataClient::syncDailyIfNeeded(true)) {
    QuoteDataClient::loadCached(homeQuote);
    requestUpdate();
  }
}

Rect HomeActivity::quoteRect(const int pageWidth, const int pageHeight, const int menuItemCount) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect menuRect = homeMenuRect(pageWidth, pageHeight, menuItemCount);
  const int top = menuRect.y + menuRect.height + HOME_QUOTE_TOP_MARGIN;
  const int bottom = pageHeight - metrics.buttonHintsHeight - GfxRenderer::VIEWABLE_MARGIN_BOTTOM - metrics.verticalSpacing;
  const int height = std::min(HOME_QUOTE_HEIGHT, std::max(72, bottom - top));
  return Rect{metrics.contentSidePadding, top, pageWidth - metrics.contentSidePadding * 2, height};
}

Rect HomeActivity::homeMenuRect(const int pageWidth, const int pageHeight, const int menuItemCount) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int menuTop = metrics.homeTopPadding + metrics.homeCoverTileHeight + HOME_MENU_VERTICAL_PADDING;
  const int bottom = pageHeight - metrics.buttonHintsHeight - GfxRenderer::VIEWABLE_MARGIN_BOTTOM - metrics.verticalSpacing;
  const int desiredHeight = homeMenuHeight(menuItemCount);
  const int maxHeight = std::max(0, bottom - menuTop - HOME_QUOTE_TOP_MARGIN - HOME_QUOTE_HEIGHT);
  return Rect{0, menuTop, pageWidth, std::min(desiredHeight, maxHeight)};
}

void HomeActivity::drawHomeQuote(const Rect rect) const {
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height, true);
  const int titleY = rect.y + 8;
  renderer.drawText(SMALL_FONT_ID, rect.x + 10, titleY, tr(STR_APP_DAILY_QUOTE), true, EpdFontFamily::BOLD);

  if (homeQuote.type[0] != '\0') {
    const std::string type = renderer.truncatedText(SMALL_FONT_ID, homeQuote.type, rect.width / 2);
    const int typeWidth = renderer.getTextWidth(SMALL_FONT_ID, type.c_str());
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - 10 - typeWidth, titleY, type.c_str());
  }

  if (!homeQuote.hasAny) {
    renderer.drawText(UI_10_FONT_ID, rect.x + 10, titleY + renderer.getLineHeight(SMALL_FONT_ID) + 10,
                      tr(STR_QUOTE_EMPTY));
    return;
  }

  const int quoteTop = titleY + renderer.getLineHeight(SMALL_FONT_ID) + 8;
  const int maxWidth = rect.width - 20;
  const int maxLines = std::max(1, (rect.y + rect.height - quoteTop - 8) / (renderer.getLineHeight(UI_10_FONT_ID) + 3));
  const auto lines = renderer.wrappedText(UI_10_FONT_ID, homeQuote.text, maxWidth, maxLines);
  int y = quoteTop;
  for (const auto& line : lines) {
    renderer.drawText(UI_10_FONT_ID, rect.x + 10, y, line.c_str());
    y += renderer.getLineHeight(UI_10_FONT_ID) + 3;
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

  const Rect menuRect = homeMenuRect(pageWidth, pageHeight, static_cast<int>(menuItems.size()));
  const Rect homeQuoteRect = quoteRect(pageWidth, pageHeight, static_cast<int>(menuItems.size()));
  drawHomeMenu(menuRect, menuItems);

  drawHomeQuote(homeQuoteRect);

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

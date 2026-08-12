#include "AppSuiteActivity.h"

#include <GfxRenderer.h>

#include <algorithm>

#include "AppPlaceholderActivity.h"
#include "ClockCalendarActivity.h"
#include "DailyQuoteActivity.h"
#include "MappedInputManager.h"
#include "WoodenFishActivity.h"
#include "activities/settings/OtaUpdateActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void AppSuiteActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

StrId AppSuiteActivity::labelFor(const BuiltInApp app) const {
  switch (app) {
    case BuiltInApp::Reader:
      return StrId::STR_APP_READER;
    case BuiltInApp::RecentBooks:
      return StrId::STR_MENU_RECENT_BOOKS;
    case BuiltInApp::Gallery:
      return StrId::STR_APP_GALLERY;
    case BuiltInApp::Notepad:
      return StrId::STR_APP_NOTEPAD;
    case BuiltInApp::Drawing:
      return StrId::STR_APP_DRAWING;
    case BuiltInApp::Music:
      return StrId::STR_APP_MUSIC;
    case BuiltInApp::Weather:
      return StrId::STR_APP_WEATHER;
    case BuiltInApp::DailyQuote:
      return StrId::STR_APP_DAILY_QUOTE;
    case BuiltInApp::ClockCalendar:
      return StrId::STR_APP_CLOCK_CALENDAR;
    case BuiltInApp::WoodenFish:
      return StrId::STR_APP_WOODEN_FISH;
    case BuiltInApp::FileManager:
      return StrId::STR_APP_FILE_MANAGER;
    case BuiltInApp::FileTransfer:
      return StrId::STR_FILE_TRANSFER;
    case BuiltInApp::Update:
      return StrId::STR_APP_UPDATE;
    case BuiltInApp::Settings:
      return StrId::STR_SETTINGS_TITLE;
    case BuiltInApp::Count:
    default:
      return StrId::STR_NONE_OPT;
  }
}

StrId AppSuiteActivity::valueFor(const BuiltInApp app) const {
  switch (app) {
    case BuiltInApp::Reader:
    case BuiltInApp::RecentBooks:
    case BuiltInApp::ClockCalendar:
    case BuiltInApp::WoodenFish:
    case BuiltInApp::FileManager:
    case BuiltInApp::FileTransfer:
    case BuiltInApp::Update:
    case BuiltInApp::Settings:
      return StrId::STR_APP_READY;
    case BuiltInApp::Gallery:
    case BuiltInApp::Notepad:
    case BuiltInApp::Drawing:
    case BuiltInApp::Weather:
      return StrId::STR_APP_STAGED;
    case BuiltInApp::DailyQuote:
      return StrId::STR_APP_READY;
    case BuiltInApp::Music:
      return StrId::STR_APP_NEEDS_HARDWARE;
    case BuiltInApp::Count:
    default:
      return StrId::STR_NONE_OPT;
  }
}

void AppSuiteActivity::openPlaceholder(const StrId title, const StrId message) {
  startActivityForResult(std::make_unique<AppPlaceholderActivity>(renderer, mappedInput, title, message), nullptr);
}

void AppSuiteActivity::activateSelected() {
  switch (itemAt(selectedIndex)) {
    case BuiltInApp::Reader:
      activityManager.goToFileBrowser();
      break;
    case BuiltInApp::RecentBooks:
      activityManager.goToRecentBooks();
      break;
    case BuiltInApp::Gallery:
      activityManager.goToFileBrowser("/pictures");
      break;
    case BuiltInApp::Notepad:
      openPlaceholder(StrId::STR_APP_NOTEPAD, StrId::STR_APP_NOTEPAD_PLACEHOLDER);
      break;
    case BuiltInApp::Drawing:
      openPlaceholder(StrId::STR_APP_DRAWING, StrId::STR_APP_DRAWING_PLACEHOLDER);
      break;
    case BuiltInApp::Music:
      openPlaceholder(StrId::STR_APP_MUSIC, StrId::STR_APP_MUSIC_PLACEHOLDER);
      break;
    case BuiltInApp::Weather:
      openPlaceholder(StrId::STR_APP_WEATHER, StrId::STR_APP_WEATHER_PLACEHOLDER);
      break;
    case BuiltInApp::DailyQuote:
      startActivityForResult(std::make_unique<DailyQuoteActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::ClockCalendar:
      startActivityForResult(std::make_unique<ClockCalendarActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::WoodenFish:
      startActivityForResult(std::make_unique<WoodenFishActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::FileManager:
      activityManager.goToFileBrowser();
      break;
    case BuiltInApp::FileTransfer:
      activityManager.goToFileTransfer();
      break;
    case BuiltInApp::Update:
      startActivityForResult(std::make_unique<OtaUpdateActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::Settings:
      activityManager.goToSettings();
      break;
    case BuiltInApp::Count:
      break;
  }
}

int AppSuiteActivity::hitTestGrid(const Rect rect, const int touchX, const int touchY) const {
  if (touchX < rect.x || touchX >= rect.x + rect.width || touchY < rect.y || touchY >= rect.y + rect.height) {
    return -1;
  }
  const int cellW = rect.width / gridCols();
  const int cellH = rect.height / gridRows();
  if (cellW <= 0 || cellH <= 0) {
    return -1;
  }
  const int col = std::min((touchX - rect.x) / cellW, gridCols() - 1);
  const int row = std::min((touchY - rect.y) / cellH, gridRows() - 1);
  const int pageStart = (selectedIndex / gridItemsPerPage()) * gridItemsPerPage();
  const int itemIndex = pageStart + row * gridCols() + col;
  return itemIndex < itemCount() ? itemIndex : -1;
}

void AppSuiteActivity::drawAppIcon(const BuiltInApp app, const int centerX, const int topY, const int size,
                                   const bool selected) const {
  const bool color = !selected;
  const int left = centerX - size / 2;
  const int right = centerX + size / 2;
  const int midY = topY + size / 2;
  const int bottom = topY + size;
  const int q = size / 4;
  const int h = size / 2;

  switch (app) {
    case BuiltInApp::Reader:
    case BuiltInApp::RecentBooks:
      renderer.drawRect(left + 2, topY + 2, h - 2, size - 4, color);
      renderer.drawRect(centerX, topY + 2, h - 2, size - 4, color);
      renderer.drawLine(centerX, topY + 4, centerX, bottom - 4, color);
      break;
    case BuiltInApp::Gallery:
      renderer.drawRect(left + 2, topY + 4, size - 4, size - 8, color);
      renderer.drawLine(left + 6, bottom - 8, centerX - 2, midY + 2, color);
      renderer.drawLine(centerX - 2, midY + 2, right - 6, bottom - 8, color);
      renderer.drawRect(right - 11, topY + 8, 5, 5, color);
      break;
    case BuiltInApp::Notepad:
      renderer.drawRect(left + 5, topY + 2, size - 10, size - 4, color);
      renderer.drawLine(left + 9, topY + 9, right - 9, topY + 9, color);
      renderer.drawLine(left + 9, topY + 16, right - 9, topY + 16, color);
      renderer.drawLine(left + 9, topY + 23, right - 13, topY + 23, color);
      break;
    case BuiltInApp::Drawing:
      renderer.drawLine(left + 7, bottom - 7, right - 8, topY + 8, 3, color);
      renderer.drawLine(left + 5, bottom - 5, left + 11, bottom - 8, color);
      renderer.drawRect(right - 10, topY + 5, 6, 6, color);
      break;
    case BuiltInApp::Music:
      renderer.drawLine(centerX - 3, topY + 8, centerX - 3, bottom - 8, color);
      renderer.drawLine(centerX - 3, topY + 8, right - 7, topY + 5, color);
      renderer.drawLine(right - 7, topY + 5, right - 7, bottom - 12, color);
      renderer.drawRect(left + 6, bottom - 10, 8, 6, color);
      renderer.drawRect(right - 12, bottom - 14, 8, 6, color);
      break;
    case BuiltInApp::Weather:
      renderer.drawRect(left + 7, midY - 2, size - 14, q + 3, color);
      renderer.drawLine(left + 10, midY - 5, left + 15, midY - 10, color);
      renderer.drawLine(left + 15, midY - 10, centerX, midY - 7, color);
      renderer.drawLine(centerX, midY - 7, right - 10, midY - 3, color);
      renderer.drawLine(left + 11, bottom - 8, left + 8, bottom - 4, color);
      renderer.drawLine(centerX, bottom - 8, centerX - 3, bottom - 4, color);
      renderer.drawLine(right - 11, bottom - 8, right - 14, bottom - 4, color);
      break;
    case BuiltInApp::DailyQuote:
      renderer.drawLine(left + 6, topY + 8, right - 6, topY + 8, color);
      renderer.drawLine(left + 6, topY + 8, left + 6, bottom - 8, color);
      renderer.drawLine(right - 6, topY + 8, right - 6, bottom - 8, color);
      renderer.drawLine(left + 6, bottom - 8, right - 6, bottom - 8, color);
      renderer.drawLine(left + 11, topY + 15, right - 11, topY + 15, color);
      renderer.drawLine(left + 11, topY + 21, centerX + q, topY + 21, color);
      renderer.drawLine(left + 11, topY + 27, right - 13, topY + 27, color);
      break;
    case BuiltInApp::ClockCalendar:
      renderer.drawRect(left + 4, topY + 6, size - 8, size - 10, color);
      renderer.drawLine(left + 4, topY + 14, right - 4, topY + 14, color);
      renderer.drawLine(centerX, midY - 2, centerX, midY + 7, color);
      renderer.drawLine(centerX, midY + 7, right - 10, midY + 7, color);
      break;
    case BuiltInApp::WoodenFish:
      renderer.drawRect(left + 3, midY - 8, size - 6, 16, 2, color);
      renderer.drawLine(left + 8, midY, right - 8, midY, color);
      renderer.drawLine(centerX + q, topY + 5, right - 3, topY + 12, color);
      break;
    case BuiltInApp::FileManager:
      renderer.drawRect(left + 4, topY + 9, size - 8, size - 13, color);
      renderer.drawLine(left + 4, topY + 9, left + 12, topY + 3, color);
      renderer.drawLine(left + 12, topY + 3, centerX, topY + 9, color);
      break;
    case BuiltInApp::FileTransfer:
      renderer.drawRect(left + 5, topY + 5, size - 10, size - 10, color);
      renderer.drawLine(centerX, topY + 10, centerX, bottom - 10, color);
      renderer.drawLine(centerX, topY + 10, centerX - 5, topY + 15, color);
      renderer.drawLine(centerX, topY + 10, centerX + 5, topY + 15, color);
      renderer.drawLine(centerX, bottom - 10, centerX - 5, bottom - 15, color);
      renderer.drawLine(centerX, bottom - 10, centerX + 5, bottom - 15, color);
      break;
    case BuiltInApp::Update:
      renderer.drawRect(left + 5, topY + 5, size - 10, size - 10, color);
      renderer.drawLine(centerX, topY + 10, centerX, bottom - 12, color);
      renderer.drawLine(centerX, topY + 10, centerX - 6, topY + 17, color);
      renderer.drawLine(centerX, topY + 10, centerX + 6, topY + 17, color);
      renderer.drawLine(left + 10, bottom - 8, right - 10, bottom - 8, color);
      break;
    case BuiltInApp::Settings:
      renderer.drawRect(centerX - 4, topY + 4, 8, size - 8, color);
      renderer.drawRect(left + 4, midY - 4, size - 8, 8, color);
      renderer.drawRect(centerX - 9, midY - 9, 18, 18, color);
      break;
    case BuiltInApp::Count:
      break;
  }
}

void AppSuiteActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const Rect gridRect{metrics.contentSidePadding, contentTop, pageWidth - metrics.contentSidePadding * 2, contentHeight};
  constexpr int pageItems = gridItemsPerPage();

  if (mappedInput.wasContentSwipedUp() || mappedInput.wasContentSwipedDown()) {
    if (pageItems > 0 && itemCount() > pageItems) {
      selectedIndex = mappedInput.wasContentSwipedUp()
                          ? ButtonNavigator::nextPageIndex(selectedIndex, itemCount(), pageItems)
                          : ButtonNavigator::previousPageIndex(selectedIndex, itemCount(), pageItems);
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasContentTapped()) {
    const int tappedIndex = hitTestGrid(gridRect, mappedInput.getTouchX(), mappedInput.getTouchY());
    if (tappedIndex >= 0) {
      selectedIndex = tappedIndex;
      activateSelected();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    activityManager.goHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelected();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
    requestUpdate();
  });
}

void AppSuiteActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const Rect gridRect{metrics.contentSidePadding, contentTop, pageWidth - metrics.contentSidePadding * 2, contentHeight};
  const int cellW = gridRect.width / gridCols();
  const int cellH = gridRect.height / gridRows();
  const int pageStart = (selectedIndex / gridItemsPerPage()) * gridItemsPerPage();
  const int pageEnd = std::min(pageStart + gridItemsPerPage(), itemCount());

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_SUITE));

  for (int i = pageStart; i < pageEnd; ++i) {
    const int local = i - pageStart;
    const int row = local / gridCols();
    const int col = local % gridCols();
    const int x = gridRect.x + col * cellW + 4;
    const int y = gridRect.y + row * cellH + 3;
    const int w = cellW - 8;
    const int h = cellH - 6;
    const bool selected = i == selectedIndex;
    renderer.fillRect(x, y, w, h, selected);
    renderer.drawRect(x, y, w, h, 2, !selected);

    const BuiltInApp app = itemAt(i);
    const int iconSize = std::min(24, std::max(18, h / 4));
    const int iconY = y + std::max(5, h / 12);
    drawAppIcon(app, x + w / 2, iconY, iconSize, selected);

    const int textMaxWidth = w - 8;
    const std::string title = renderer.truncatedText(UI_10_FONT_ID, I18N.get(labelFor(app)), textMaxWidth,
                                                     EpdFontFamily::BOLD);
    const int titleWidth = renderer.getTextWidth(UI_10_FONT_ID, title.c_str(), EpdFontFamily::BOLD);
    const int titleX = x + (w - titleWidth) / 2;
    const int titleY = iconY + iconSize + 4;
    renderer.drawText(UI_10_FONT_ID, titleX, titleY, title.c_str(), !selected, EpdFontFamily::BOLD);

    const int valueY = titleY + renderer.getLineHeight(UI_10_FONT_ID) + 2;
    if (valueY + renderer.getLineHeight(SMALL_FONT_ID) < y + h - 2) {
      const std::string value = renderer.truncatedText(SMALL_FONT_ID, I18N.get(valueFor(app)), textMaxWidth);
      const int valueWidth = renderer.getTextWidth(SMALL_FONT_ID, value.c_str());
      const int valueX = x + (w - valueWidth) / 2;
      renderer.drawText(SMALL_FONT_ID, valueX, valueY, value.c_str(), !selected);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

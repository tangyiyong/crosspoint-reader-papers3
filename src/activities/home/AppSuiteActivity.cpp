#include "AppSuiteActivity.h"

#include <GfxRenderer.h>

#include <algorithm>

#include "ClockCalendarActivity.h"
#include "DailyQuoteActivity.h"
#include "HotNewsActivity.h"
#include "MappedInputManager.h"
#include "AlarmActivity.h"
#include "NotepadActivity.h"
#include "PomodoroActivity.h"
#include "TodayHistoryActivity.h"
#include "WeatherActivity.h"
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
    case BuiltInApp::Weather:
      return StrId::STR_APP_WEATHER;
    case BuiltInApp::DailyQuote:
      return StrId::STR_APP_DAILY_QUOTE;
    case BuiltInApp::TodayHistory:
      return StrId::STR_TODAY_HISTORY;
    case BuiltInApp::HotNews:
      return StrId::STR_APP_HOT_NEWS;
    case BuiltInApp::Notepad:
      return StrId::STR_APP_NOTEPAD;
    case BuiltInApp::Pomodoro:
      return StrId::STR_APP_POMODORO;
    case BuiltInApp::Alarm:
      return StrId::STR_APP_ALARM;
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
    case BuiltInApp::Weather:
    case BuiltInApp::Notepad:
    case BuiltInApp::Pomodoro:
    case BuiltInApp::Alarm:
      return StrId::STR_NONE_OPT;
    case BuiltInApp::Gallery:
      return StrId::STR_APP_STAGED;
    case BuiltInApp::DailyQuote:
    case BuiltInApp::TodayHistory:
    case BuiltInApp::HotNews:
      return StrId::STR_NONE_OPT;
    case BuiltInApp::Count:
    default:
      return StrId::STR_NONE_OPT;
  }
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
    case BuiltInApp::Weather:
      startActivityForResult(std::make_unique<WeatherActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::DailyQuote:
      startActivityForResult(std::make_unique<DailyQuoteActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::TodayHistory:
      startActivityForResult(std::make_unique<TodayHistoryActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::HotNews:
      startActivityForResult(std::make_unique<HotNewsActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::Notepad:
      startActivityForResult(std::make_unique<NotepadActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::Pomodoro:
      startActivityForResult(std::make_unique<PomodoroActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::Alarm:
      startActivityForResult(std::make_unique<AlarmActivity>(renderer, mappedInput), nullptr);
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
    case BuiltInApp::Count:
      break;
  }
}

int AppSuiteActivity::hitTestGrid(const Rect rect, const int touchX, const int touchY) const {
  if (touchX < rect.x || touchX >= rect.x + rect.width || touchY < rect.y || touchY >= rect.y + rect.height) {
    return -1;
  }
  const int cellW = rect.width / gridCols();
  const int cellH = gridCellHeight();
  if (cellW <= 0 || cellH <= 0) {
    return -1;
  }
  if (touchY >= rect.y + gridRows() * cellH) {
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
      renderer.drawLine(centerX, topY + 4, centerX, bottom - 4, color);
      renderer.drawLine(centerX, topY + 4, left + 4, topY + 7, 2, color);
      renderer.drawLine(left + 4, topY + 7, left + 4, bottom - 5, 2, color);
      renderer.drawLine(left + 4, bottom - 5, centerX, bottom - 3, 2, color);
      renderer.drawLine(centerX, topY + 4, right - 4, topY + 7, 2, color);
      renderer.drawLine(right - 4, topY + 7, right - 4, bottom - 5, 2, color);
      renderer.drawLine(right - 4, bottom - 5, centerX, bottom - 3, 2, color);
      renderer.drawLine(left + 8, topY + 14, centerX - 4, topY + 12, color);
      renderer.drawLine(centerX + 4, topY + 12, right - 8, topY + 14, color);
      break;
    case BuiltInApp::Gallery:
      renderer.drawRect(left + 3, topY + 9, size - 6, size - 14, 2, color);
      renderer.drawRect(left + 10, topY + 5, size / 3, 6, color);
      renderer.drawRect(centerX - 7, midY - 7, 14, 14, 2, color);
      renderer.drawRect(right - 11, topY + 14, 5, 5, color);
      break;
    case BuiltInApp::Weather:
      renderer.drawRect(left + 6, topY + 7, 10, 10, 2, color);
      renderer.drawLine(left + 11, topY + 2, left + 11, topY + 5, color);
      renderer.drawLine(left + 11, topY + 19, left + 11, topY + 22, color);
      renderer.drawLine(left + 1, topY + 12, left + 4, topY + 12, color);
      renderer.drawLine(left + 18, topY + 12, left + 21, topY + 12, color);
      renderer.drawRect(left + 12, midY - 2, size - 18, q + 8, 2, color);
      renderer.drawLine(left + 16, midY - 4, left + 22, midY - 10, 2, color);
      renderer.drawLine(left + 22, midY - 10, centerX + 3, midY - 6, 2, color);
      renderer.drawLine(centerX + 3, midY - 6, right - 7, midY - 2, 2, color);
      break;
    case BuiltInApp::DailyQuote:
      renderer.drawLine(left + 6, topY + 8, right - 6, topY + 8, color);
      renderer.drawLine(left + 6, topY + 8, left + 6, bottom - 8, color);
      renderer.drawLine(right - 6, topY + 8, right - 6, bottom - 8, color);
      renderer.drawLine(left + 6, bottom - 8, right - 6, bottom - 8, color);
      renderer.drawLine(left + 11, topY + 15, right - 11, topY + 15, color);
      renderer.drawLine(left + 11, topY + 21, centerX + q, topY + 21, color);
      renderer.drawLine(left + 11, topY + 27, right - 13, topY + 27, color);
      renderer.drawLine(left + 13, bottom - 13, centerX - 3, bottom - 7, color);
      renderer.drawLine(centerX - 3, bottom - 7, right - 10, bottom - 16, color);
      break;
    case BuiltInApp::TodayHistory:
      renderer.drawRect(left + 5, topY + 8, size - 10, size - 12, 2, color);
      renderer.drawLine(left + 5, topY + 17, right - 5, topY + 17, color);
      renderer.drawLine(left + 13, topY + 4, left + 13, topY + 11, color);
      renderer.drawLine(right - 13, topY + 4, right - 13, topY + 11, color);
      renderer.drawLine(left + 11, topY + 25, right - 10, topY + 25, color);
      renderer.drawLine(left + 11, topY + 31, centerX + q, topY + 31, color);
      break;
    case BuiltInApp::HotNews:
      renderer.drawRect(left + 5, topY + 5, size - 10, size - 10, 2, color);
      renderer.fillRect(left + 10, topY + 10, 8, 8, color);
      renderer.drawLine(left + 22, topY + 11, right - 9, topY + 11, 2, color);
      renderer.drawLine(left + 10, topY + 23, right - 9, topY + 23, color);
      renderer.drawLine(left + 10, topY + 30, centerX + q, topY + 30, color);
      break;
    case BuiltInApp::Notepad:
      renderer.drawRect(left + 7, topY + 4, size - 14, size - 8, 2, color);
      renderer.drawLine(right - 13, topY + 4, right - 7, topY + 10, color);
      renderer.drawLine(left + 12, topY + 14, right - 12, topY + 14, color);
      renderer.drawLine(left + 12, topY + 22, right - 12, topY + 22, color);
      renderer.drawLine(left + 12, topY + 30, centerX + q, topY + 30, color);
      break;
    case BuiltInApp::Pomodoro:
      renderer.drawRect(centerX - 11, topY + 7, 22, size - 11, 2, color);
      renderer.drawLine(centerX - 5, topY + 3, centerX + 5, topY + 3, color);
      renderer.drawLine(centerX, topY + 14, centerX, midY + 7, color);
      renderer.drawLine(centerX, midY + 7, centerX + 7, midY + 2, color);
      break;
    case BuiltInApp::Alarm:
      renderer.drawRect(left + 8, topY + 10, size - 16, size - 14, 2, color);
      renderer.drawLine(centerX, midY - 2, centerX, midY + 8, color);
      renderer.drawLine(centerX, midY + 8, right - 11, midY + 8, color);
      renderer.drawLine(left + 9, topY + 7, left + 16, topY + 2, 2, color);
      renderer.drawLine(right - 9, topY + 7, right - 16, topY + 2, 2, color);
      renderer.drawLine(left + 15, bottom - 4, left + 11, bottom, color);
      renderer.drawLine(right - 15, bottom - 4, right - 11, bottom, color);
      break;
    case BuiltInApp::ClockCalendar:
      renderer.drawRect(left + 5, topY + 6, size - 10, size - 12, 2, color);
      renderer.drawLine(left + 5, topY + 16, right - 5, topY + 16, color);
      renderer.drawLine(left + 12, topY + 24, left + 12, topY + 30, color);
      renderer.drawLine(centerX, topY + 24, centerX, topY + 30, color);
      renderer.drawLine(right - 12, topY + 24, right - 12, topY + 30, color);
      break;
    case BuiltInApp::WoodenFish:
      renderer.drawRect(left + 4, midY - 8, size - 8, 16, 2, color);
      renderer.drawLine(left + 10, midY, right - 10, midY, 2, color);
      renderer.drawLine(centerX + 3, topY + 7, right - 2, topY + 15, 2, color);
      renderer.drawLine(right - 7, topY + 5, right - 2, topY + 15, color);
      break;
    case BuiltInApp::FileManager:
      renderer.drawRect(left + 4, topY + 11, size - 8, size - 16, 2, color);
      renderer.drawLine(left + 4, topY + 11, left + 14, topY + 4, 2, color);
      renderer.drawLine(left + 14, topY + 4, centerX + 5, topY + 11, 2, color);
      renderer.drawLine(left + 10, midY + 5, right - 10, midY + 5, color);
      break;
    case BuiltInApp::FileTransfer:
      renderer.drawRect(left + 5, topY + 5, size - 10, size - 10, 2, color);
      renderer.drawLine(centerX, topY + 10, centerX, bottom - 10, 2, color);
      renderer.drawLine(centerX, topY + 10, centerX - 6, topY + 16, 2, color);
      renderer.drawLine(centerX, topY + 10, centerX + 6, topY + 16, 2, color);
      renderer.drawLine(centerX, bottom - 10, centerX - 6, bottom - 16, 2, color);
      renderer.drawLine(centerX, bottom - 10, centerX + 6, bottom - 16, 2, color);
      break;
    case BuiltInApp::Update:
      renderer.drawRect(left + 5, topY + 5, size - 10, size - 10, 2, color);
      renderer.drawLine(centerX, topY + 11, centerX, bottom - 14, 2, color);
      renderer.drawLine(centerX, bottom - 14, centerX - 7, bottom - 21, 2, color);
      renderer.drawLine(centerX, bottom - 14, centerX + 7, bottom - 21, 2, color);
      renderer.drawLine(left + 10, bottom - 8, right - 10, bottom - 8, 2, color);
      break;
    case BuiltInApp::Count:
      break;
  }
}

void AppSuiteActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 3;
  const int contentHeight = std::min(gridRows() * gridCellHeight(),
                                     pageHeight - contentTop - metrics.buttonHintsHeight -
                                         GfxRenderer::VIEWABLE_MARGIN_BOTTOM - metrics.verticalSpacing * 2);
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
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 3;
  const int contentHeight = std::min(gridRows() * gridCellHeight(),
                                     pageHeight - contentTop - metrics.buttonHintsHeight -
                                         GfxRenderer::VIEWABLE_MARGIN_BOTTOM - metrics.verticalSpacing * 2);
  const Rect gridRect{metrics.contentSidePadding, contentTop, pageWidth - metrics.contentSidePadding * 2, contentHeight};
  const int cellW = gridRect.width / gridCols();
  const int cellH = gridCellHeight();
  const int pageStart = (selectedIndex / gridItemsPerPage()) * gridItemsPerPage();
  const int pageEnd = std::min(pageStart + gridItemsPerPage(), itemCount());

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_SUITE));

  for (int i = pageStart; i < pageEnd; ++i) {
    const int local = i - pageStart;
    const int row = local / gridCols();
    const int col = local % gridCols();
    const int x = gridRect.x + col * cellW + 3;
    const int y = gridRect.y + row * cellH + 3;
    const int w = cellW - 6;
    const int h = cellH - 8;
    const bool selected = i == selectedIndex;
    renderer.fillRect(x, y, w, h, selected);
    renderer.drawRect(x, y, w, h, 2, !selected);

    const BuiltInApp app = itemAt(i);
    const int iconSize = std::min(38, std::max(28, h / 3));
    const int iconY = y + 10;
    drawAppIcon(app, x + w / 2, iconY, iconSize, selected);

    const int textMaxWidth = w - 8;
    const int titleY = iconY + iconSize + 4;
    const auto titleLines = renderer.wrappedText(UI_10_FONT_ID, I18N.get(labelFor(app)), textMaxWidth, 2,
                                                 EpdFontFamily::BOLD);
    int titleLineY = titleY;
    for (const auto& line : titleLines) {
      const int titleWidth = renderer.getTextWidth(UI_10_FONT_ID, line.c_str(), EpdFontFamily::BOLD);
      const int titleX = x + (w - titleWidth) / 2;
      renderer.drawText(UI_10_FONT_ID, titleX, titleLineY, line.c_str(), !selected, EpdFontFamily::BOLD);
      titleLineY += renderer.getLineHeight(UI_10_FONT_ID) + 1;
    }

    const StrId valueId = valueFor(app);
    const int valueY = titleLineY + 1;
    if (valueId != StrId::STR_NONE_OPT && valueY + renderer.getLineHeight(SMALL_FONT_ID) < y + h - 2) {
      const std::string value = renderer.truncatedText(SMALL_FONT_ID, I18N.get(valueId), textMaxWidth);
      const int valueWidth = renderer.getTextWidth(SMALL_FONT_ID, value.c_str());
      const int valueX = x + (w - valueWidth) / 2;
      renderer.drawText(SMALL_FONT_ID, valueX, valueY, value.c_str(), !selected);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

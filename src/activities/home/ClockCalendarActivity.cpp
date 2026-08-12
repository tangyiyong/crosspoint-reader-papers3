#include "ClockCalendarActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <ctime>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "calendar/CalendarDataClient.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
time_t getLocalDisplayTime() {
  const time_t now = time(nullptr);
  if (now <= 1700000000) {
    return 0;
  }

  const int offsetQuarterHours = static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48;
  return now + offsetQuarterHours * 15 * 60;
}

bool getLocalDate(struct tm& timeinfo) {
  const time_t localNow = getLocalDisplayTime();
  if (localNow == 0) {
    return false;
  }
  gmtime_r(&localNow, &timeinfo);
  return true;
}

bool formatDate(char* buf, const size_t bufSize) {
  struct tm timeinfo;
  if (!getLocalDate(timeinfo)) {
    return false;
  }
  snprintf(buf, bufSize, "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  return true;
}

bool isLeapYear(const int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(const int year, const int month) {
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year)) {
    return 29;
  }
  return days[month - 1];
}

int weekdayMondayFirst(const int year, const int month, const int day) {
  int y = year;
  int m = month;
  if (m < 3) {
    m += 12;
    y -= 1;
  }
  const int k = y % 100;
  const int j = y / 100;
  const int h = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
  const int sundayFirst = (h + 6) % 7;
  return (sundayFirst + 6) % 7;
}

const char* weekdayLabel(const int index) {
  static constexpr StrId labels[] = {StrId::STR_WEEK_MON, StrId::STR_WEEK_TUE, StrId::STR_WEEK_WED,
                                     StrId::STR_WEEK_THU, StrId::STR_WEEK_FRI, StrId::STR_WEEK_SAT,
                                     StrId::STR_WEEK_SUN};
  return I18N.get(labels[index]);
}

Rect calendarGridRect(const GfxRenderer& renderer) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + 84;
  const int bottom = pageHeight - metrics.buttonHintsHeight - 12;
  return Rect{metrics.contentSidePadding, top, pageWidth - metrics.contentSidePadding * 2,
              std::max(210, bottom - top)};
}
}  // namespace

void ClockCalendarActivity::onEnter() {
  Activity::onEnter();
  if (loadCurrentLocalDate()) {
    displayYear = todayYear;
    displayMonth = todayMonth;
    selectedDay = todayDay;
  }
  requestUpdate();
}

void ClockCalendarActivity::loop() {
  if (!monthSyncAttempted && !monthSyncing && displayYear > 0 && displayMonth > 0) {
    syncDisplayedMonthIfNeeded();
    return;
  }

  if (showingDayDetail &&
      (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasContentTapped())) {
    showingDayDetail = false;
    requestUpdate();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
      mappedInput.wasPressed(MappedInputManager::Button::Up) || mappedInput.wasReaderSwipeRight()) {
    changeMonth(-1);
    requestUpdate();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right) ||
      mappedInput.wasPressed(MappedInputManager::Button::Down) || mappedInput.wasReaderSwipeLeft()) {
    changeMonth(1);
    requestUpdate();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && selectedDay > 0) {
    showingDayDetail = true;
    requestUpdate();
    return;
  }

  if (mappedInput.wasContentTapped()) {
    const int day = hitTestDay(mappedInput.getTouchX(), mappedInput.getTouchY());
    if (day > 0) {
      selectedDay = day;
      showingDayDetail = true;
      requestUpdate();
    }
  }
}

void ClockCalendarActivity::render(RenderLock&&) {
  drawMonthCalendar();
}

bool ClockCalendarActivity::loadCurrentLocalDate() {
  struct tm timeinfo;
  if (!getLocalDate(timeinfo)) {
    return false;
  }

  todayYear = timeinfo.tm_year + 1900;
  todayMonth = timeinfo.tm_mon + 1;
  todayDay = timeinfo.tm_mday;
  return true;
}

void ClockCalendarActivity::changeMonth(const int delta) {
  if (displayYear <= 0 || displayMonth <= 0) {
    if (!loadCurrentLocalDate()) return;
    displayYear = todayYear;
    displayMonth = todayMonth;
  }

  displayMonth += delta;
  while (displayMonth < 1) {
    displayMonth += 12;
    displayYear -= 1;
  }
  while (displayMonth > 12) {
    displayMonth -= 12;
    displayYear += 1;
  }
  selectedDay = std::min(selectedDay > 0 ? selectedDay : 1, daysInMonth(displayYear, displayMonth));
  monthSyncAttempted = false;
}

void ClockCalendarActivity::syncDisplayedMonthIfNeeded() {
  monthSyncAttempted = true;
  if (!CalendarDataClient::hasConfiguredApi()) {
    return;
  }

  monthSyncing = true;
  requestUpdateAndWait();
  CalendarDataClient::syncMonth(displayYear, displayMonth);
  monthSyncing = false;
  requestUpdate();
}

int ClockCalendarActivity::hitTestDay(const int touchX, const int touchY) const {
  if (displayYear <= 0 || displayMonth <= 0) {
    return 0;
  }

  const Rect grid = calendarGridRect(renderer);
  const int headerHeight = 30;
  const int cellW = grid.width / 7;
  const int cellH = (grid.height - headerHeight) / 6;
  const int firstWeekday = weekdayMondayFirst(displayYear, displayMonth, 1);
  const int days = daysInMonth(displayYear, displayMonth);
  const int y = touchY - (grid.y + headerHeight);
  if (touchX < grid.x || touchX >= grid.x + cellW * 7 || y < 0 || y >= cellH * 6) {
    return 0;
  }

  const int col = (touchX - grid.x) / cellW;
  const int row = y / cellH;
  const int day = row * 7 + col - firstWeekday + 1;
  return day >= 1 && day <= days ? day : 0;
}

void ClockCalendarActivity::drawMonthCalendar() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();

  char timeBuf[12];
  const bool hasTime = halClock.formatTime(timeBuf, sizeof(timeBuf), SETTINGS.clockUtcOffsetQ, SETTINGS.clockFormat == 1);
  char dateBuf[16];
  const bool hasDate = formatDate(dateBuf, sizeof(dateBuf));

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_CLOCK_CALENDAR));

  const int titleY = metrics.topPadding + metrics.headerHeight + 14;
  char monthBuf[24];
  if (displayYear > 0 && displayMonth > 0) {
    snprintf(monthBuf, sizeof(monthBuf), "%04d-%02d", displayYear, displayMonth);
  } else {
    snprintf(monthBuf, sizeof(monthBuf), "--");
  }
  renderer.drawCenteredText(UI_12_FONT_ID, titleY, monthBuf, true, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, titleY + 32,
                    monthSyncing ? tr(STR_CALENDAR_SYNCING) : (hasTime ? timeBuf : tr(STR_CLOCK_UNAVAILABLE)));
  const char* rightText = hasDate ? dateBuf : tr(STR_DATE_UNAVAILABLE);
  const int dateWidth = renderer.getTextWidth(UI_10_FONT_ID, rightText);
  renderer.drawText(UI_10_FONT_ID, pageWidth - metrics.contentSidePadding - dateWidth, titleY + 32, rightText);

  const Rect grid = calendarGridRect(renderer);
  const int headerHeight = 30;
  const int cellW = grid.width / 7;
  const int cellH = (grid.height - headerHeight) / 6;
  const int firstWeekday = displayYear > 0 ? weekdayMondayFirst(displayYear, displayMonth, 1) : 0;
  const int days = displayYear > 0 ? daysInMonth(displayYear, displayMonth) : 0;

  for (int i = 0; i < 7; i++) {
    const int x = grid.x + i * cellW;
    const int w = (i == 6) ? (grid.width - i * cellW) : cellW;
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, weekdayLabel(i));
    renderer.drawText(UI_10_FONT_ID, x + (w - textWidth) / 2, grid.y + 4, weekdayLabel(i), true,
                      EpdFontFamily::BOLD);
  }

  renderer.drawLine(grid.x, grid.y + headerHeight, grid.x + grid.width, grid.y + headerHeight);
  for (int i = 1; i < 7; i++) {
    const int x = grid.x + i * cellW;
    renderer.drawLine(x, grid.y + headerHeight, x, grid.y + grid.height);
  }
  for (int i = 1; i <= 6; i++) {
    const int y = grid.y + headerHeight + i * cellH;
    renderer.drawLine(grid.x, y, grid.x + grid.width, y);
  }

  for (int day = 1; day <= days; day++) {
    const int index = firstWeekday + day - 1;
    const int col = index % 7;
    const int row = index / 7;
    const int x = grid.x + col * cellW;
    const int y = grid.y + headerHeight + row * cellH;
    const bool isToday = displayYear == todayYear && displayMonth == todayMonth && day == todayDay;
    char dayBuf[4];
    snprintf(dayBuf, sizeof(dayBuf), "%d", day);

    if (isToday) {
      renderer.fillRect(x + 2, y + 2, cellW - 4, cellH - 4);
    }
    const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, dayBuf);
    renderer.drawText(UI_12_FONT_ID, x + (cellW - textWidth) / 2,
                      y + (cellH - renderer.getLineHeight(UI_12_FONT_ID)) / 2, dayBuf, !isToday,
                      isToday ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  }

  if (showingDayDetail) {
    drawDayDetail();
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DETAILS), tr(STR_PREVIOUS), tr(STR_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void ClockCalendarActivity::drawDayDetail() const {
  if (displayYear <= 0 || displayMonth <= 0 || selectedDay <= 0) {
    return;
  }

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int popupW = std::min(pageWidth - 56, 420);
  const int popupH = 220;
  const int popupX = (pageWidth - popupW) / 2;
  const int popupY = (pageHeight - popupH) / 2;

  renderer.fillRect(popupX, popupY, popupW, popupH, false);
  renderer.drawRect(popupX, popupY, popupW, popupH, 2, true);

  char title[24];
  snprintf(title, sizeof(title), "%04d-%02d-%02d", displayYear, displayMonth, selectedDay);
  const int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, title, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, popupX + (popupW - titleWidth) / 2, popupY + 20, title, true,
                    EpdFontFamily::BOLD);

  const int weekday = weekdayMondayFirst(displayYear, displayMonth, selectedDay);
  char weekdayLine[40];
  snprintf(weekdayLine, sizeof(weekdayLine), "%s: %s", tr(STR_WEEKDAY), weekdayLabel(weekday));
  renderer.drawText(UI_10_FONT_ID, popupX + 28, popupY + 68, weekdayLine);

  CalendarDayInfo info;
  const bool hasInfo = CalendarDataClient::loadDayInfo(displayYear, displayMonth, selectedDay, info);
  if (!hasInfo) {
    renderer.drawText(UI_10_FONT_ID, popupX + 28, popupY + 100, tr(STR_CALENDAR_CACHE_MISSING));
    renderer.drawText(UI_10_FONT_ID, popupX + 28, popupY + 132, tr(STR_LUNAR_PENDING));
    renderer.drawText(UI_10_FONT_ID, popupX + 28, popupY + 164, tr(STR_TAP_TO_CLOSE));
    return;
  }

  char line[112];
  snprintf(line, sizeof(line), "%s: %s", tr(STR_LUNAR), info.lunar[0] ? info.lunar : "-");
  renderer.drawText(UI_10_FONT_ID, popupX + 28, popupY + 100, line);
  const char* label = info.solarTerm[0] ? tr(STR_SOLAR_TERM) : tr(STR_FESTIVAL);
  const char* text = info.solarTerm[0] ? info.solarTerm : (info.festival[0] ? info.festival : "-");
  snprintf(line, sizeof(line), "%s: %s", label, text);
  renderer.drawText(UI_10_FONT_ID, popupX + 28, popupY + 124, line);
  snprintf(line, sizeof(line), "%s: %s", tr(STR_ALMANAC_GOOD), info.almanacGood[0] ? info.almanacGood : "-");
  renderer.drawText(UI_10_FONT_ID, popupX + 28, popupY + 148, line);
  snprintf(line, sizeof(line), "%s: %s", tr(STR_ALMANAC_BAD), info.almanacBad[0] ? info.almanacBad : "-");
  renderer.drawText(UI_10_FONT_ID, popupX + 28, popupY + 172, line);
}

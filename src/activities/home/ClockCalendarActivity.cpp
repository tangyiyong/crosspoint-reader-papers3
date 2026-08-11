#include "ClockCalendarActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <ctime>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
bool formatDate(char* buf, const size_t bufSize) {
  const time_t now = time(nullptr);
  if (now <= 1700000000) {
    return false;
  }

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  snprintf(buf, bufSize, "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  return true;
}
}  // namespace

void ClockCalendarActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void ClockCalendarActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
  }
}

void ClockCalendarActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int midY = pageHeight / 2;

  char timeBuf[12];
  const bool hasTime = halClock.formatTime(timeBuf, sizeof(timeBuf), SETTINGS.clockUtcOffsetQ, SETTINGS.clockFormat == 1);
  char dateBuf[16];
  const bool hasDate = formatDate(dateBuf, sizeof(dateBuf));

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_CLOCK_CALENDAR));
  renderer.drawCenteredText(UI_12_FONT_ID, midY - 56, hasTime ? timeBuf : tr(STR_CLOCK_UNAVAILABLE), true,
                            EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, midY - 16, hasDate ? dateBuf : tr(STR_DATE_UNAVAILABLE));
  renderer.drawCenteredText(UI_10_FONT_ID, midY + 24, tr(STR_CLOCK_CALENDAR_HINT));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#include "AlarmActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void AlarmActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void AlarmActivity::adjust(const int delta) {
  delayMinutes = static_cast<uint8_t>(std::clamp(static_cast<int>(delayMinutes) + delta, 1, 120));
  if (armed) {
    dueAt = millis() + static_cast<unsigned long>(delayMinutes) * 60UL * 1000UL;
  }
  requestUpdate();
}

long AlarmActivity::remainingSeconds() const {
  if (!armed) return static_cast<long>(delayMinutes) * 60L;
  const long remaining = static_cast<long>((dueAt - millis()) / 1000UL);
  return remaining < 0 ? 0 : remaining;
}

void AlarmActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasContentTapped()) {
    armed = !armed;
    dueAt = millis() + static_cast<unsigned long>(delayMinutes) * 60UL * 1000UL;
    requestUpdate();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    adjust(5);
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    adjust(-5);
    return;
  }
  if (armed && remainingSeconds() == 0) {
    armed = false;
    requestUpdate();
  }
}

void AlarmActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const long remaining = remainingSeconds();
  char timeBuf[24];
  snprintf(timeBuf, sizeof(timeBuf), "%02ld:%02ld", remaining / 60L, remaining % 60L);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_ALARM));
  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 58, armed ? tr(STR_ALARM_ARMED) : tr(STR_ALARM_IDLE), true,
                            EpdFontFamily::BOLD);
  renderer.drawCenteredText(BOOKERLY_18_FONT_ID, pageHeight / 2 - 12, timeBuf, true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 42, tr(STR_ALARM_HINT));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), armed ? tr(STR_STOP) : tr(STR_START), "+5", "-5");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

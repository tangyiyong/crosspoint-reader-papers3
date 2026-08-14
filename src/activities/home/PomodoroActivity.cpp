#include "PomodoroActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void PomodoroActivity::onEnter() {
  Activity::onEnter();
  reset();
  requestUpdate();
}

unsigned long PomodoroActivity::remainingMs() const {
  if (!running) return durationMs;
  const unsigned long elapsed = millis() - startedAt;
  return elapsed >= durationMs ? 0 : durationMs - elapsed;
}

void PomodoroActivity::toggle() {
  if (running) {
    durationMs = remainingMs();
    running = false;
  } else {
    startedAt = millis();
    running = true;
  }
  requestUpdate();
}

void PomodoroActivity::reset() {
  running = false;
  breakMode = false;
  durationMs = WORK_MS;
}

void PomodoroActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasContentTapped()) {
    toggle();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    reset();
    requestUpdate();
    return;
  }
  if (running && remainingMs() == 0) {
    running = false;
    breakMode = !breakMode;
    durationMs = breakMode ? BREAK_MS : WORK_MS;
    requestUpdate();
  }
}

void PomodoroActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const unsigned long rem = remainingMs() / 1000UL;
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu", rem / 60UL, rem % 60UL);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_POMODORO));
  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 64, breakMode ? tr(STR_POMODORO_BREAK) : tr(STR_POMODORO_WORK),
                            true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(BOOKERLY_18_FONT_ID, pageHeight / 2 - 18, timeBuf, true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 36, running ? tr(STR_RUNNING) : tr(STR_PAUSED));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), running ? tr(STR_PAUSE) : tr(STR_START), tr(STR_RESET), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

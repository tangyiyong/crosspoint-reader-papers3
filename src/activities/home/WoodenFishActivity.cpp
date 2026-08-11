#include "WoodenFishActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
void playWoodenFishSound() {
#if CROSSPOINT_ENABLE_AUDIO
  // Reserved for an external I2S codec or buzzer driver.
#endif
}
}  // namespace

void WoodenFishActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void WoodenFishActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }

  if (mappedInput.wasContentTapped() || mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    ++count;
    playWoodenFishSound();
    requestUpdate();
  }
}

void WoodenFishActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int cx = pageWidth / 2;
  const int cy = pageHeight / 2;

  char countBuf[24];
  snprintf(countBuf, sizeof(countBuf), "%lu", static_cast<unsigned long>(count));

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_WOODEN_FISH));
  renderer.fillRect(cx - 95, cy - 48, 190, 96, false);
  renderer.drawRect(cx - 95, cy - 48, 190, 96, 3, true);
  renderer.drawCenteredText(UI_12_FONT_ID, cy - 16, tr(STR_WOODEN_FISH_KNOCK), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, cy + 20, countBuf);
  renderer.drawCenteredText(UI_10_FONT_ID, cy + 72, tr(STR_WOODEN_FISH_HINT));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

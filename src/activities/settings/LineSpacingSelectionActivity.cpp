#include "LineSpacingSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kSmallStep = 1;
constexpr int kLargeStep = 10;
}  // namespace

void LineSpacingSelectionActivity::onEnter() {
  Activity::onEnter();
  value = std::clamp(value, static_cast<int>(CrossPointSettings::LINE_SPACING_MIN),
                     static_cast<int>(CrossPointSettings::LINE_SPACING_MAX));
  requestUpdate();
}

void LineSpacingSelectionActivity::onExit() { Activity::onExit(); }

void LineSpacingSelectionActivity::adjustValue(const int delta) {
  value = std::clamp(value + delta, static_cast<int>(CrossPointSettings::LINE_SPACING_MIN),
                     static_cast<int>(CrossPointSettings::LINE_SPACING_MAX));
  requestUpdate();
}

void LineSpacingSelectionActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    setResult(PercentResult{value});
    finish();
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { adjustValue(-kSmallStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { adjustValue(kSmallStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] { adjustValue(kLargeStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] { adjustValue(-kLargeStep); });
}

void LineSpacingSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const bool isPortraitInverted = renderer.getOrientation() == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterHeight = isPortraitInverted ? (metrics.buttonHintsHeight + metrics.verticalSpacing) : 0;

  renderer.drawCenteredText(UI_12_FONT_ID, 15 + hintGutterHeight, tr(STR_LINE_SPACING), true, EpdFontFamily::BOLD);

  char valueBuf[16];
  snprintf(valueBuf, sizeof(valueBuf), "%.2fx", static_cast<float>(value) / 100.0f);
  renderer.drawCenteredText(UI_12_FONT_ID, 90 + hintGutterHeight, valueBuf, true, EpdFontFamily::BOLD);

  const int screenWidth = renderer.getScreenWidth();
  constexpr int barWidth = 360;
  constexpr int barHeight = 16;
  const int barX = (screenWidth - barWidth) / 2;
  const int barY = 140 + hintGutterHeight;

  renderer.drawRect(barX, barY, barWidth, barHeight);

  constexpr int range = CrossPointSettings::LINE_SPACING_MAX - CrossPointSettings::LINE_SPACING_MIN;
  const int normalized = value - CrossPointSettings::LINE_SPACING_MIN;
  const int fillWidth = (barWidth - 4) * normalized / range;
  if (fillWidth > 0) {
    renderer.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4);
  }

  const int knobX = barX + 2 + fillWidth - 2;
  renderer.fillRect(knobX, barY - 4, 4, barHeight + 8, true);

  renderer.drawCenteredText(SMALL_FONT_ID, barY + 30, tr(STR_PERCENT_STEP_HINT), true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "-", "+");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

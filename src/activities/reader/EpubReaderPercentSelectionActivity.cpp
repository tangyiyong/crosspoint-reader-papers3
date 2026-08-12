#include "EpubReaderPercentSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Fine/coarse slider step sizes for percent adjustments.
constexpr int kSmallStep = 1;
constexpr int kLargeStep = 10;
constexpr int kSliderWidth = 360;
constexpr int kSliderHeight = 16;
constexpr int kSliderY = 140;
constexpr int kSliderTouchPadding = 24;
}  // namespace

void EpubReaderPercentSelectionActivity::onEnter() {
  Activity::onEnter();
  // Set up rendering task and mark first frame dirty.
  requestUpdate();
}

void EpubReaderPercentSelectionActivity::onExit() { Activity::onExit(); }

void EpubReaderPercentSelectionActivity::adjustPercent(const int delta) {
  // Apply delta and clamp within 0-100.
  percent += delta;
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }
  requestUpdate();
}

void EpubReaderPercentSelectionActivity::setPercentFromTouch(const int touchX, const int touchY) {
  const int screenWidth = renderer.getScreenWidth();
  const int barX = (screenWidth - kSliderWidth) / 2;
  if (touchX < barX || touchX > barX + kSliderWidth || touchY < kSliderY - kSliderTouchPadding ||
      touchY > kSliderY + kSliderHeight + kSliderTouchPadding) {
    return;
  }

  percent = (touchX - barX) * 100 / kSliderWidth;
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }
  requestUpdate();
}

void EpubReaderPercentSelectionActivity::loop() {
  // Back cancels, confirm selects, arrows adjust the percent.
  if (mappedInput.wasContentTapped()) {
    setPercentFromTouch(mappedInput.getTouchX(), mappedInput.getTouchY());
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    setResult(PercentResult{percent});
    finish();
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { adjustPercent(-kSmallStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { adjustPercent(kSmallStep); });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] { adjustPercent(kLargeStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] { adjustPercent(-kLargeStep); });
}

void EpubReaderPercentSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  // Title and numeric percent value.
  renderer.drawCenteredText(UI_12_FONT_ID, 15, tr(STR_GO_TO_PERCENT), true, EpdFontFamily::BOLD);

  const std::string percentText = std::to_string(percent) + "%";
  renderer.drawCenteredText(UI_12_FONT_ID, 90, percentText.c_str(), true, EpdFontFamily::BOLD);

  // Draw slider track.
  const int screenWidth = renderer.getScreenWidth();
  const int barX = (screenWidth - kSliderWidth) / 2;

  renderer.drawRect(barX, kSliderY, kSliderWidth, kSliderHeight);

  // Fill slider based on percent.
  const int fillWidth = (kSliderWidth - 4) * percent / 100;
  if (fillWidth > 0) {
    renderer.fillRect(barX + 2, kSliderY + 2, fillWidth, kSliderHeight - 4);
  }

  // Draw a simple knob centered at the current percent.
  const int knobX = barX + 2 + fillWidth - 2;
  renderer.fillRect(knobX, kSliderY - 4, 4, kSliderHeight + 8, true);

  // Hint text for step sizes.
  renderer.drawCenteredText(SMALL_FONT_ID, kSliderY + 30, tr(STR_PERCENT_STEP_HINT), true);

  // Button hints follow the current front button layout.
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "-", "+");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

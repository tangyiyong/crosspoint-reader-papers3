#include "UsbMassStorageActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/UsbMassStorageService.h"

void UsbMassStorageActivity::onEnter() {
  Activity::onEnter();
  started = USB_MASS_STORAGE.begin();
  requestUpdate();
}

void UsbMassStorageActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    ESP.restart();
  }
}

void UsbMassStorageActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_USB_MASS_STORAGE));

  const int centerY = pageHeight / 2;
  if (started) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - 70, tr(STR_USB_MSC_READY), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - 30, tr(STR_USB_MSC_HINT_1), true);
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - 5, tr(STR_USB_MSC_HINT_2), true);
    renderer.drawCenteredText(UI_10_FONT_ID, centerY + 35, tr(STR_USB_MSC_RESTART_HINT), true);
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - 45, tr(STR_USB_MSC_FAILED), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - 10, USB_MASS_STORAGE.lastError(), true);
    renderer.drawCenteredText(UI_10_FONT_ID, centerY + 35, tr(STR_USB_MSC_RESTART_HINT), true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OK_BUTTON), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

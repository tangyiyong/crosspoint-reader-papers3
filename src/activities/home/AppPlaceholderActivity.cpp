#include "AppPlaceholderActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void AppPlaceholderActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void AppPlaceholderActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
  }
}

void AppPlaceholderActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int midY = pageHeight / 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, I18N.get(title));
  renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, I18N.get(title), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, midY + 12, I18N.get(message));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

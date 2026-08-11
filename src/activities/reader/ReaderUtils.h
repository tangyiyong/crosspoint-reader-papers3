#pragma once

#include <CrossPointSettings.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace ReaderUtils {

constexpr unsigned long GO_HOME_MS = 1000;
constexpr unsigned long READER_MENU_LONG_PRESS_MS = 650;

inline void applyOrientation(GfxRenderer& renderer, const uint8_t orientation) {
  switch (orientation) {
    case CrossPointSettings::ORIENTATION::PORTRAIT:
      renderer.setOrientation(GfxRenderer::Orientation::Portrait);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
      break;
    case CrossPointSettings::ORIENTATION::INVERTED:
      renderer.setOrientation(GfxRenderer::Orientation::PortraitInverted);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CCW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
      break;
    default:
      break;
  }
}

struct PageTurnResult {
  bool prev;
  bool next;
};

enum class QuickBarAction : uint8_t { None, Back, Home, Settings };

inline PageTurnResult detectPageTurn(const MappedInputManager& input) {
  const bool usePress = !SETTINGS.longPressChapterSkip;
  const bool prev = input.wasReaderSwipeRight() ||
                    (usePress ? (input.wasPressed(MappedInputManager::Button::PageBack) ||
                                input.wasPressed(MappedInputManager::Button::Left))
                             : (input.wasReleased(MappedInputManager::Button::PageBack) ||
                                input.wasReleased(MappedInputManager::Button::Left)));
  const bool powerTurn = SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::PAGE_TURN &&
                         input.wasReleased(MappedInputManager::Button::Power);
  const bool next = input.wasReaderSwipeLeft() ||
                    (usePress ? (input.wasPressed(MappedInputManager::Button::PageForward) || powerTurn ||
                                input.wasPressed(MappedInputManager::Button::Right))
                             : (input.wasReleased(MappedInputManager::Button::PageForward) || powerTurn ||
                                input.wasReleased(MappedInputManager::Button::Right)));
  return {prev, next};
}

inline bool wasBackGesture(const MappedInputManager& input) {
  return input.wasReaderSwipeRightFromLeftEdge();
}

inline bool wasBackRevealGesture(const MappedInputManager& input) { return input.wasReaderSwipeDownFromTopEdge(); }

inline bool wasBackOverlayTap(const MappedInputManager& input) { return input.wasReaderTapTop(); }

inline bool wasMenuGesture(const MappedInputManager& input) { return input.wasReaderSwipeUpFromBottomEdge(); }

inline int quickBarButtonWidth(const GfxRenderer& renderer) {
  return renderer.getScreenWidth() / 3;
}

inline int quickBarHeight() { return 54; }

inline int quickBarX(const GfxRenderer& renderer) {
  (void)renderer;
  return 0;
}

inline QuickBarAction hitTestReaderQuickBar(const GfxRenderer& renderer, const bool visible, const int16_t touchX,
                                            const int16_t touchY) {
  if (!visible) {
    return QuickBarAction::None;
  }

  constexpr int buttonY = 10;
  const int screenWidth = renderer.getScreenWidth();
  if (touchY < buttonY || touchY >= buttonY + quickBarHeight() || touchX < 0 || touchX >= screenWidth) {
    return QuickBarAction::None;
  }
  const int index = (touchX * 3) / screenWidth;
  if (index == 0) return QuickBarAction::Back;
  if (index == 1) return QuickBarAction::Home;
  return QuickBarAction::Settings;
}

inline void drawReaderBackOverlay(const GfxRenderer& renderer, const bool visible) {
  if (!visible) {
    return;
  }

  constexpr int buttonY = 10;
  constexpr int cornerRadius = 5;
  const int screenWidth = renderer.getScreenWidth();
  const char* labels[] = {tr(STR_BACK), tr(STR_HOME), tr(STR_SETTINGS_TITLE)};

  for (int i = 0; i < 3; ++i) {
    const int buttonX = i * screenWidth / 3;
    const int nextX = (i + 1) * screenWidth / 3;
    const int buttonWidth = nextX - buttonX;
    renderer.fillRect(buttonX, buttonY, buttonWidth, quickBarHeight(), false);
    renderer.drawRoundedRect(buttonX, buttonY, buttonWidth, quickBarHeight(), 1, cornerRadius, true);
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, labels[i]);
    const int textX = buttonX + (buttonWidth - textWidth) / 2;
    const int textY = buttonY + (quickBarHeight() - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawText(UI_10_FONT_ID, textX, textY, labels[i], true);
  }
}

inline void displayWithRefreshCycle(const GfxRenderer& renderer, int& pagesUntilFullRefresh) {
  if (pagesUntilFullRefresh <= 1) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
  } else {
    renderer.displayBuffer();
    pagesUntilFullRefresh--;
  }
}

// Single-pass grayscale anti-aliasing. Sets GRAYSCALE_DIRECT mode so the
// render callback writes EPD gray values (0-3) directly into the 8bpp
// framebuffer. No LSB/MSB bitplane conversion needed — EPD_Painter accepts
// 0-3 natively. Caller is responsible for displaying the buffer afterwards.
template <typename RenderFn>
void renderAntiAliased(GfxRenderer& renderer, RenderFn&& renderFn) {
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_DIRECT);
  renderFn();
  renderer.setRenderMode(GfxRenderer::BW);
}

}  // namespace ReaderUtils

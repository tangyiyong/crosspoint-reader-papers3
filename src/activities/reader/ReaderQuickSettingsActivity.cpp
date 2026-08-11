#include "ReaderQuickSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/settings/FontSelectActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void ReaderQuickSettingsActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void ReaderQuickSettingsActivity::onExit() {
  if (changed) {
    SETTINGS.saveToFile();
  }
  Activity::onExit();
}

StrId ReaderQuickSettingsActivity::labelFor(const Item item) const {
  switch (item) {
    case Item::ColorMode:
      return StrId::STR_COLOR_MODE;
#if CROSSPOINT_PAPERS3
    case Item::ExternalFont:
      return StrId::STR_EXTERNAL_FONT;
#endif
    case Item::FontSize:
      return StrId::STR_FONT_SIZE;
    case Item::LineSpacing:
      return StrId::STR_LINE_SPACING;
    case Item::ScreenMargin:
      return StrId::STR_SCREEN_MARGIN;
    case Item::ParagraphAlignment:
      return StrId::STR_PARA_ALIGNMENT;
    case Item::Orientation:
      return StrId::STR_ORIENTATION;
    case Item::FirstLineIndent:
      return StrId::STR_FIRST_LINE_INDENT;
    case Item::TextAntiAliasing:
      return StrId::STR_TEXT_AA;
    case Item::Images:
      return StrId::STR_IMAGES;
    case Item::ItemCount:
    default:
      return StrId::STR_NONE_OPT;
  }
}

std::string ReaderQuickSettingsActivity::valueFor(const Item item) const {
  switch (item) {
    case Item::ColorMode:
      return I18N.get(SETTINGS.colorMode == CrossPointSettings::COLOR_MODE::DARK_MODE ? StrId::STR_DARK
                                                                                       : StrId::STR_LIGHT);
#if CROSSPOINT_PAPERS3
    case Item::ExternalFont:
      return "";
#endif
    case Item::FontSize: {
      static constexpr StrId labels[] = {StrId::STR_SMALL, StrId::STR_MEDIUM, StrId::STR_LARGE, StrId::STR_X_LARGE};
      return I18N.get(labels[SETTINGS.fontSize]);
    }
    case Item::LineSpacing: {
      char buf[16];
      snprintf(buf, sizeof(buf), "%.2fx", static_cast<float>(SETTINGS.lineSpacing) / 100.0f);
      return buf;
    }
    case Item::ScreenMargin:
      return std::to_string(SETTINGS.screenMargin);
    case Item::ParagraphAlignment: {
      static constexpr StrId labels[] = {StrId::STR_JUSTIFY, StrId::STR_ALIGN_LEFT, StrId::STR_CENTER,
                                         StrId::STR_ALIGN_RIGHT, StrId::STR_BOOK_S_STYLE};
      return I18N.get(labels[SETTINGS.paragraphAlignment]);
    }
    case Item::Orientation: {
#if CROSSPOINT_PAPERS3
      return I18N.get(SETTINGS.orientation == CrossPointSettings::LANDSCAPE_CCW ? StrId::STR_LANDSCAPE_CCW
                                                                                : StrId::STR_PORTRAIT);
#else
      static constexpr StrId labels[] = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW, StrId::STR_INVERTED,
                                         StrId::STR_LANDSCAPE_CCW};
      return I18N.get(labels[SETTINGS.orientation]);
#endif
    }
    case Item::FirstLineIndent:
      return I18N.get(SETTINGS.firstLineIndent ? StrId::STR_ON_MARKER : StrId::STR_STATE_OFF);
    case Item::TextAntiAliasing:
      return I18N.get(SETTINGS.textAntiAliasing ? StrId::STR_ON_MARKER : StrId::STR_STATE_OFF);
    case Item::Images: {
      static constexpr StrId labels[] = {StrId::STR_IMAGES_DISPLAY, StrId::STR_IMAGES_PLACEHOLDER,
                                         StrId::STR_IMAGES_SUPPRESS};
      return I18N.get(labels[SETTINGS.imageRendering]);
    }
    case Item::ItemCount:
    default:
      return "";
  }
}

void ReaderQuickSettingsActivity::markChanged() {
  changed = true;
  requestUpdate();
}

void ReaderQuickSettingsActivity::adjustSelected(const int delta) {
  const Item item = itemAt(selectedIndex);
  if (item == Item::LineSpacing) {
    const int next = std::clamp(static_cast<int>(SETTINGS.lineSpacing) + delta,
                                static_cast<int>(CrossPointSettings::LINE_SPACING_MIN),
                                static_cast<int>(CrossPointSettings::LINE_SPACING_MAX));
    if (next != SETTINGS.lineSpacing) {
      SETTINGS.lineSpacing = static_cast<uint8_t>(next);
      markChanged();
    }
    return;
  }
  if (item == Item::ScreenMargin) {
    const int next = std::clamp(static_cast<int>(SETTINGS.screenMargin) + delta, 5, 40);
    if (next != SETTINGS.screenMargin) {
      SETTINGS.screenMargin = static_cast<uint8_t>(next);
      markChanged();
    }
  }
}

void ReaderQuickSettingsActivity::activateSelected() {
  const Item item = itemAt(selectedIndex);
  switch (item) {
    case Item::ColorMode:
      SETTINGS.colorMode = (SETTINGS.colorMode + 1) % CrossPointSettings::COLOR_MODE_COUNT;
      renderer.setDarkMode(SETTINGS.colorMode == CrossPointSettings::COLOR_MODE::DARK_MODE);
      markChanged();
      break;
#if CROSSPOINT_PAPERS3
    case Item::ExternalFont:
      startActivityForResult(std::make_unique<FontSelectActivity>(renderer, mappedInput), [this](const ActivityResult&) {
        changed = true;
        requestUpdate();
      });
      break;
#endif
    case Item::FontSize:
      SETTINGS.fontSize = (SETTINGS.fontSize + 1) % CrossPointSettings::FONT_SIZE_COUNT;
      markChanged();
      break;
    case Item::LineSpacing:
      adjustSelected(10);
      break;
    case Item::ScreenMargin:
      adjustSelected(5);
      break;
    case Item::ParagraphAlignment:
      SETTINGS.paragraphAlignment = (SETTINGS.paragraphAlignment + 1) % CrossPointSettings::PARAGRAPH_ALIGNMENT_COUNT;
      markChanged();
      break;
    case Item::Orientation:
#if CROSSPOINT_PAPERS3
      SETTINGS.orientation = CrossPointSettings::nextPaperS3Orientation(SETTINGS.orientation);
#else
      SETTINGS.orientation = (SETTINGS.orientation + 1) % CrossPointSettings::ORIENTATION_COUNT;
#endif
      markChanged();
      break;
    case Item::FirstLineIndent:
      SETTINGS.firstLineIndent = !SETTINGS.firstLineIndent;
      markChanged();
      break;
    case Item::TextAntiAliasing:
      SETTINGS.textAntiAliasing = !SETTINGS.textAntiAliasing;
      markChanged();
      break;
    case Item::Images:
      SETTINGS.imageRendering = (SETTINGS.imageRendering + 1) % CrossPointSettings::IMAGE_RENDERING_COUNT;
      markChanged();
      break;
    case Item::ItemCount:
      break;
  }
}

void ReaderQuickSettingsActivity::finishWithResult(const bool cancelled) {
  ActivityResult result;
  result.isCancelled = cancelled;
  setResult(std::move(result));
  finish();
}

void ReaderQuickSettingsActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const Rect listRect{0, contentTop, pageWidth, contentHeight};
  const int pageItems = UITheme::getInstance().getListItemsPerPage(listRect, false);

  if (mappedInput.wasContentSwipedUp() || mappedInput.wasContentSwipedDown()) {
    if (pageItems > 0 && itemCount() > pageItems) {
      selectedIndex = mappedInput.wasContentSwipedUp()
                          ? ButtonNavigator::nextPageIndex(selectedIndex, itemCount(), pageItems)
                          : ButtonNavigator::previousPageIndex(selectedIndex, itemCount(), pageItems);
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasContentTapped()) {
    const int tappedIndex = UITheme::getInstance().hitTestListItem(listRect, itemCount(), selectedIndex, false,
                                                                   mappedInput.getTouchX(), mappedInput.getTouchY());
    if (tappedIndex >= 0) {
      selectedIndex = tappedIndex;
      activateSelected();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finishWithResult(!changed);
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    activateSelected();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { adjustSelected(-5); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { adjustSelected(5); });
}

void ReaderQuickSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CAT_READER));
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount(), selectedIndex,
      [this](int index) { return std::string(I18N.get(labelFor(itemAt(index)))); },
      nullptr, nullptr, [this](int index) { return valueFor(itemAt(index)); });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "-", "+");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

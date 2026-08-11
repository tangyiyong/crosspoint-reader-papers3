#include "FontSelectActivity.h"

#include <ExternalFont.h>
#include <FontManager.h>
#include <I18n.h>
#include <Logging.h>

#include <memory>
#include <string>
#include <utility>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "activities/util/FontPreviewActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ExternalFontLabel.h"

void FontSelectActivity::onEnter() {
  Activity::onEnter();

  FontMgr.scanFonts();
  totalItems = 1 + FontMgr.getFontCount();

  const int selectedExternal = FontMgr.getSelectedIndex();
  selectedIndex = selectedExternal < 0 ? 0 : selectedExternal + 1;
  requestUpdate();
}

void FontSelectActivity::loop() {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (mappedInput.wasContentTapped()) {
    const int tappedIndex = UITheme::getInstance().hitTestListItem(
        Rect{0, contentTop, pageWidth, contentHeight}, totalItems, selectedIndex, false, mappedInput.getTouchX(),
        mappedInput.getTouchY());
    if (tappedIndex >= 0) {
      selectedIndex = tappedIndex;
      applySelection();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    applySelection();
    return;
  }

  bool needsRender = false;
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
      mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    selectedIndex = (selectedIndex + totalItems - 1) % totalItems;
    needsRender = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down) ||
             mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    selectedIndex = (selectedIndex + 1) % totalItems;
    needsRender = true;
  }

  if (needsRender) {
    requestUpdate();
  }
}

void FontSelectActivity::applySelection() {
  if (selectedIndex == 0) {
    FontMgr.selectFont(-1);
    renderer.setReaderFallbackFontId(SETTINGS.getBuiltInReaderFontId());
    ActivityResult result;
    result.isCancelled = false;
    setResult(std::move(result));
    finish();
    return;
  }

  const int externalIndex = selectedIndex - 1;
  const FontInfo* info = FontMgr.getFontInfo(externalIndex);
  if (!info) {
    LOG_ERR("FNT", "Font index not found: %d", externalIndex);
    return;
  }
  if (!ExternalFont::canFitGlyph(info->width, info->height)) {
    LOG_ERR("FNT", "Font glyph too large: %s (%ux%u)", info->name, info->width, info->height);
    return;
  }

  startActivityForResult(std::make_unique<FontPreviewActivity>(renderer, mappedInput, externalIndex),
                         [this](const ActivityResult& result) {
                           if (!result.isCancelled) {
                             ActivityResult appliedResult;
                             appliedResult.isCancelled = false;
                             setResult(std::move(appliedResult));
                             finish();
                             return;
                           }
                           requestUpdate();
                         });
}

void FontSelectActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_EXTERNAL_FONT));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int currentIndex = FontMgr.getSelectedIndex() < 0 ? 0 : FontMgr.getSelectedIndex() + 1;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, totalItems, selectedIndex,
      [](int i) -> std::string {
        if (i == 0) {
          return std::string(tr(STR_BUILTIN_FONT));
        }
        const FontInfo* info = FontMgr.getFontInfo(i - 1);
        if (!info) {
          return "";
        }
        return buildExternalFontLabel(info->filename, info->name, info->size,
                                      ExternalFont::canFitGlyph(info->width, info->height));
      },
      nullptr, nullptr,
      [currentIndex](int i) -> std::string { return i == currentIndex ? std::string(tr(STR_ON_MARKER)) : ""; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

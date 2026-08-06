#include "FontPreviewActivity.h"

#include <ExternalFont.h>
#include <FontManager.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstddef>
#include <string>
#include <utility>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ExternalFontLabel.h"

namespace {
constexpr StrId kSampleLines[] = {StrId::STR_FONT_PREVIEW_SAMPLE_CJK, StrId::STR_FONT_PREVIEW_SAMPLE_JA,
                                  StrId::STR_FONT_PREVIEW_SAMPLE_LATIN};
constexpr size_t kSampleLineCount = sizeof(kSampleLines) / sizeof(kSampleLines[0]);
}  // namespace

void FontPreviewActivity::onEnter() {
  Activity::onEnter();

  originalReaderFontIndex = FontMgr.getSelectedIndex();
  originalUiFontIndex = FontMgr.getUiSelectedIndex();

  if (canPreviewSelectedFont()) {
    previewInstalled = FontMgr.previewFont(fontIndex);
    renderer.setReaderFallbackFontId(SETTINGS.getBuiltInReaderFontId());
  }

  requestUpdate();
}

void FontPreviewActivity::onExit() {
  restorePreviewFont();
  Activity::onExit();
}

bool FontPreviewActivity::canPreviewSelectedFont() const {
  const FontInfo* info = FontMgr.getFontInfo(fontIndex);
  return info && ExternalFont::canFitGlyph(info->width, info->height);
}

void FontPreviewActivity::restorePreviewFont() {
  if (!applied && previewInstalled) {
    FontMgr.restoreFontSelection(originalReaderFontIndex, originalUiFontIndex);
    renderer.setReaderFallbackFontId(SETTINGS.getBuiltInReaderFontId());
  }
  previewInstalled = false;
}

void FontPreviewActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (!previewInstalled) {
      return;
    }
    FontMgr.saveSettings();
    applied = true;
    ActivityResult result;
    result.isCancelled = false;
    setResult(std::move(result));
    finish();
    return;
  }
}

void FontPreviewActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const FontInfo* info = FontMgr.getFontInfo(fontIndex);

  const std::string title = info ? buildExternalFontLabel(info->filename, info->name, info->size,
                                                          ExternalFont::canFitGlyph(info->width, info->height))
                                 : std::string(tr(STR_EXTERNAL_FONT));
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title.c_str(),
                 tr(STR_PREVIEW));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentLeft = metrics.contentSidePadding;
  const int contentWidth = pageWidth - metrics.contentSidePadding * 2;
  const int previewFontId = previewInstalled ? SETTINGS.getReaderFontId() : SETTINGS.getBuiltInReaderFontId();
  const int lineSpacing = renderer.getLineHeight(previewFontId) + 6;

  int y = contentTop + 10;
  for (size_t i = 0; i < kSampleLineCount && y + lineSpacing <= contentBottom; ++i) {
    const EpdFontFamily::Style style = (i == 0) ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const int remainingLines = lineSpacing > 0 ? (contentBottom - y) / lineSpacing : 0;
    const char* sampleText = I18N.get(kSampleLines[i]);
    const auto wrappedLines = renderer.wrappedText(previewFontId, sampleText, contentWidth, remainingLines, style);
    for (const auto& line : wrappedLines) {
      if (y + lineSpacing > contentBottom) {
        break;
      }
      renderer.drawText(previewFontId, contentLeft, y, line.c_str(), true, style);
      y += lineSpacing;
    }
    y += 4;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

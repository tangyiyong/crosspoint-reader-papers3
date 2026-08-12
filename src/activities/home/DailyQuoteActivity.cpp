#include "DailyQuoteActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void DailyQuoteActivity::onEnter() {
  Activity::onEnter();
  QuoteDataClient::loadCached(quote);
  refreshQuote();
}

void DailyQuoteActivity::refreshQuote() {
  syncing = true;
  lastFetchFailed = false;
  requestUpdateAndWait();
  if (!QuoteDataClient::fetchRandom(true)) {
    lastFetchFailed = true;
  }
  QuoteDataClient::loadCached(quote);
  syncing = false;
  requestUpdate();
}

void DailyQuoteActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    refreshQuote();
  }
}

void DailyQuoteActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentX = metrics.contentSidePadding;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_DAILY_QUOTE));

  renderer.drawText(UI_12_FONT_ID, contentX, top, tr(STR_APP_DAILY_QUOTE), true, EpdFontFamily::BOLD);
  const char* status = syncing ? tr(STR_LOADING) : (lastFetchFailed ? tr(STR_QUOTE_FETCH_FAILED) : quote.type);
  const std::string safeStatus = renderer.truncatedText(UI_10_FONT_ID, status, contentW);
  renderer.drawText(UI_10_FONT_ID, contentX, top + 34, safeStatus.c_str());

  const int quoteTop = top + 76;
  const char* quoteText = quote.hasAny ? quote.text : tr(STR_QUOTE_EMPTY);
  const auto lines = renderer.wrappedText(UI_12_FONT_ID, quoteText, contentW, 8);
  int y = quoteTop;
  for (const auto& line : lines) {
    renderer.drawText(UI_12_FONT_ID, contentX, y, line.c_str());
    y += renderer.getLineHeight(UI_12_FONT_ID) + 4;
  }

  if (quote.cn[0] != '\0' && y + renderer.getLineHeight(UI_10_FONT_ID) < pageHeight - metrics.buttonHintsHeight - 20) {
    const auto cnLines = renderer.wrappedText(UI_10_FONT_ID, quote.cn, contentW, 3);
    y += 10;
    for (const auto& line : cnLines) {
      renderer.drawText(UI_10_FONT_ID, contentX, y, line.c_str());
      y += renderer.getLineHeight(UI_10_FONT_ID) + 3;
    }
  }

  if (quote.date[0] != '\0') {
    const int dateWidth = renderer.getTextWidth(SMALL_FONT_ID, quote.date);
    renderer.drawText(SMALL_FONT_ID, pageWidth - metrics.contentSidePadding - dateWidth,
                      pageHeight - metrics.buttonHintsHeight - 24, quote.date);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

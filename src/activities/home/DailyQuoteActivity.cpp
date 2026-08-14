#include "DailyQuoteActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <vector>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int QUOTE_MAX_LINES = 48;
constexpr int QUOTE_LINE_GAP = 4;
constexpr int QUOTE_STATUS_TOP_OFFSET = 34;
constexpr int QUOTE_CONTENT_TOP_OFFSET = 76;

void appendLines(std::vector<std::string>& target, const std::vector<std::string>& lines) {
  target.insert(target.end(), lines.begin(), lines.end());
}
}  // namespace

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
  topLine = 0;
  syncing = false;
  requestUpdate();
}

int DailyQuoteActivity::visibleLineCount() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2 + QUOTE_CONTENT_TOP_OFFSET;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID) + QUOTE_LINE_GAP;
  return std::max(1, (bottom - top) / lineHeight);
}

int DailyQuoteActivity::totalLineCount() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  const char* quoteText = quote.hasAny ? quote.text : tr(STR_QUOTE_EMPTY);
  auto lines = renderer.wrappedText(UI_12_FONT_ID, quoteText, contentW, QUOTE_MAX_LINES);
  if (quote.cn[0] != '\0') {
    if (!lines.empty()) {
      lines.emplace_back("");
    }
    appendLines(lines, renderer.wrappedText(UI_10_FONT_ID, quote.cn, contentW, 12));
  }
  return std::max(1, static_cast<int>(lines.size()));
}

void DailyQuoteActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    refreshQuote();
    return;
  }

  const int maxTop = std::max(0, totalLineCount() - visibleLineCount());
  if (mappedInput.wasContentSwipedUp()) {
    topLine = std::min(maxTop, topLine + visibleLineCount());
    requestUpdate();
    return;
  }
  if (mappedInput.wasContentSwipedDown()) {
    topLine = std::max(0, topLine - visibleLineCount());
    requestUpdate();
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this, maxTop] {
    topLine = std::min(maxTop, topLine + 1);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    topLine = std::max(0, topLine - 1);
    requestUpdate();
  });
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
  const int visibleLines = visibleLineCount();
  const std::string safeStatus = renderer.truncatedText(UI_10_FONT_ID, status, contentW);
  renderer.drawText(UI_10_FONT_ID, contentX, top + QUOTE_STATUS_TOP_OFFSET, safeStatus.c_str());

  const int quoteTop = top + QUOTE_CONTENT_TOP_OFFSET;
  const char* quoteText = quote.hasAny ? quote.text : tr(STR_QUOTE_EMPTY);
  std::vector<std::string> lines = renderer.wrappedText(UI_12_FONT_ID, quoteText, contentW, QUOTE_MAX_LINES);
  if (quote.cn[0] != '\0') {
    if (!lines.empty()) {
      lines.emplace_back("");
    }
    appendLines(lines, renderer.wrappedText(UI_10_FONT_ID, quote.cn, contentW, 12));
  }
  topLine = std::min(topLine, std::max(0, static_cast<int>(lines.size()) - visibleLines));
  int y = quoteTop;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID) + QUOTE_LINE_GAP;
  const int endLine = std::min(static_cast<int>(lines.size()), topLine + visibleLines);
  for (int i = topLine; i < endLine; ++i) {
    if (y + renderer.getLineHeight(UI_12_FONT_ID) > bottom) {
      break;
    }
    const auto& line = lines[i];
    renderer.drawText(UI_12_FONT_ID, contentX, y, line.c_str());
    y += lineHeight;
  }

  if (quote.date[0] != '\0') {
    const int dateWidth = renderer.getTextWidth(SMALL_FONT_ID, quote.date);
    renderer.drawText(SMALL_FONT_ID, pageWidth - metrics.contentSidePadding - dateWidth,
                      pageHeight - metrics.buttonHintsHeight - 24, quote.date);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

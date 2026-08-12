#include "HotNewsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void HotNewsActivity::onEnter() {
  Activity::onEnter();
  NewsDataClient::loadCached(news);
  refreshNews();
}

void HotNewsActivity::refreshNews() {
  syncing = true;
  lastFetchFailed = false;
  requestUpdateAndWait();
  if (!NewsDataClient::fetchHotNews(true)) {
    lastFetchFailed = true;
  }
  NewsDataClient::loadCached(news);
  topIndex = std::min(topIndex, std::max(0, static_cast<int>(news.items.size()) - 1));
  syncing = false;
  requestUpdate();
}

int HotNewsActivity::visibleItemCount() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + 72;
  const int bottom = pageHeight - metrics.buttonHintsHeight - 14;
  return std::max(1, (bottom - top) / 56);
}

void HotNewsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    refreshNews();
    return;
  }

  const int maxTop = std::max(0, static_cast<int>(news.items.size()) - visibleItemCount());
  if (mappedInput.wasContentSwipedUp()) {
    topIndex = std::min(maxTop, topIndex + visibleItemCount());
    requestUpdate();
    return;
  }
  if (mappedInput.wasContentSwipedDown()) {
    topIndex = std::max(0, topIndex - visibleItemCount());
    requestUpdate();
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this, maxTop] {
    topIndex = std::min(maxTop, topIndex + 1);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    topIndex = std::max(0, topIndex - 1);
    requestUpdate();
  });
}

void HotNewsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentX = metrics.contentSidePadding;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_HOT_NEWS));

  const char* title = news.title[0] != '\0' ? news.title : tr(STR_APP_HOT_NEWS);
  const std::string safeTitle = renderer.truncatedText(UI_12_FONT_ID, title, contentW, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, contentX, top, safeTitle.c_str(), true, EpdFontFamily::BOLD);

  const char* status = syncing ? tr(STR_LOADING) : (lastFetchFailed ? tr(STR_NEWS_FETCH_FAILED) : news.date);
  const std::string safeStatus = renderer.truncatedText(UI_10_FONT_ID, status, contentW);
  renderer.drawText(UI_10_FONT_ID, contentX, top + 34, safeStatus.c_str());

  const int listTop = top + 72;
  const int listBottom = pageHeight - metrics.buttonHintsHeight - 14;
  const int rowH = 56;
  int y = listTop;
  if (!news.hasAny) {
    renderer.drawText(UI_10_FONT_ID, contentX, y, tr(STR_NEWS_EMPTY));
  } else {
    const int end = std::min(static_cast<int>(news.items.size()), topIndex + std::max(1, (listBottom - listTop) / rowH));
    for (int i = topIndex; i < end; ++i) {
      char prefix[8];
      snprintf(prefix, sizeof(prefix), "%d.", i + 1);
      renderer.drawText(SMALL_FONT_ID, contentX, y + 2, prefix, true, EpdFontFamily::BOLD);
      const int textX = contentX + 24;
      const auto lines = renderer.wrappedText(UI_10_FONT_ID, news.items[i].c_str(), contentW - 24, 2);
      int lineY = y;
      for (const auto& line : lines) {
        renderer.drawText(UI_10_FONT_ID, textX, lineY, line.c_str());
        lineY += renderer.getLineHeight(UI_10_FONT_ID) + 2;
      }
      y += rowH;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

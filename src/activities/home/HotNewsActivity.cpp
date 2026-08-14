#include "HotNewsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/WifiUtils.h"

namespace {
constexpr int NEWS_TEXT_LEFT_OFFSET = 30;
constexpr int NEWS_ITEM_GAP = 12;
constexpr int NEWS_MAX_LINES_PER_ITEM = 10;
}  // namespace

void HotNewsActivity::onEnter() {
  Activity::onEnter();
  NewsDataClient::loadCached(news);
  refreshNews();
}

void HotNewsActivity::refreshNews() {
  syncing = true;
  lastFetchFailed = false;
  requestUpdateAndWait();
  if (!WifiUtils::ensureConnected()) {
    NewsDataClient::loadCached(news);
    lastFetchFailed = true;
    syncing = false;
    requestUpdate();
    return;
  }
  if (!NewsDataClient::fetchHotNews(false)) {
    lastFetchFailed = true;
  }
  NewsDataClient::loadCached(news);
  topIndex = std::min(topIndex, std::max(0, static_cast<int>(news.items.size()) - 1));
  syncing = false;
  requestUpdate();
}

int HotNewsActivity::visibleItemCount() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + 72;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  int y = top;
  int visible = 0;
  for (int i = topIndex; i < static_cast<int>(news.items.size()); ++i) {
    const int h = itemHeight(i, contentW);
    if (visible > 0 && y + h > bottom) {
      break;
    }
    ++visible;
    y += h;
    if (y >= bottom) {
      break;
    }
  }
  return std::max(1, visible);
}

int HotNewsActivity::itemHeight(const int index, const int contentWidth) const {
  if (index < 0 || index >= static_cast<int>(news.items.size())) {
    return 0;
  }
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID) + 3;
  const auto lines =
      renderer.wrappedText(UI_10_FONT_ID, news.items[index].c_str(), contentWidth - NEWS_TEXT_LEFT_OFFSET,
                           NEWS_MAX_LINES_PER_ITEM);
  return std::max(lineHeight + NEWS_ITEM_GAP, static_cast<int>(lines.size()) * lineHeight + NEWS_ITEM_GAP);
}

int HotNewsActivity::visibleEndIndex() const {
  if (news.items.empty()) {
    return -1;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int listTop = top + 72;
  const int listBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  int y = listTop;
  int end = topIndex;
  for (int i = topIndex; i < static_cast<int>(news.items.size()); ++i) {
    const int h = itemHeight(i, contentW);
    if (i > topIndex && y + h > listBottom) {
      break;
    }
    end = i;
    y += h;
    if (y >= listBottom) {
      break;
    }
  }
  return end;
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

  const int maxTop = std::max(0, static_cast<int>(news.items.size()) - 1);
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
  char rangeText[20] = "";
  if (news.hasAny) {
    const int end = visibleEndIndex();
    snprintf(rangeText, sizeof(rangeText), "%d-%d/%d", topIndex + 1, end + 1, static_cast<int>(news.items.size()));
  }
  const int rangeWidth = rangeText[0] != '\0' ? renderer.getTextWidth(SMALL_FONT_ID, rangeText) : 0;
  const int statusMaxWidth = contentW - rangeWidth - (rangeWidth > 0 ? 12 : 0);
  const std::string safeStatus = renderer.truncatedText(UI_10_FONT_ID, status, std::max(40, statusMaxWidth));
  renderer.drawText(UI_10_FONT_ID, contentX, top + 34, safeStatus.c_str());
  if (rangeText[0] != '\0') {
    renderer.drawText(SMALL_FONT_ID, contentX + contentW - rangeWidth, top + 34, rangeText);
  }

  const int listTop = top + 72;
  const int listBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  int y = listTop;
  if (!news.hasAny) {
    renderer.drawText(UI_10_FONT_ID, contentX, y, tr(STR_NEWS_EMPTY));
  } else {
    for (int i = topIndex; i < static_cast<int>(news.items.size()); ++i) {
      const auto lines =
          renderer.wrappedText(UI_10_FONT_ID, news.items[i].c_str(), contentW - NEWS_TEXT_LEFT_OFFSET,
                               NEWS_MAX_LINES_PER_ITEM);
      const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID) + 3;
      const int neededHeight = std::max(lineHeight + NEWS_ITEM_GAP,
                                        static_cast<int>(lines.size()) * lineHeight + NEWS_ITEM_GAP);
      if (i > topIndex && y + neededHeight > listBottom) {
        break;
      }
      char prefix[8];
      snprintf(prefix, sizeof(prefix), "%d.", i + 1);
      renderer.drawText(SMALL_FONT_ID, contentX, y + 2, prefix, true, EpdFontFamily::BOLD);
      const int textX = contentX + NEWS_TEXT_LEFT_OFFSET;
      int lineY = y;
      for (const auto& line : lines) {
        if (lineY + renderer.getLineHeight(UI_10_FONT_ID) > listBottom) {
          break;
        }
        renderer.drawText(UI_10_FONT_ID, textX, lineY, line.c_str());
        lineY += lineHeight;
      }
      y += neededHeight;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

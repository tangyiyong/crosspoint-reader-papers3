#include "TodayHistoryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int TODAY_TEXT_LEFT_OFFSET = 30;
constexpr int TODAY_ITEM_GAP = 14;
constexpr int TODAY_TITLE_MAX_LINES = 2;
constexpr int TODAY_DESC_MAX_LINES = 4;
}  // namespace

void TodayHistoryActivity::onEnter() {
  Activity::onEnter();
  TodayHistoryClient::loadCached(todayHistory);
  refreshTodayHistory();
}

void TodayHistoryActivity::refreshTodayHistory() {
  syncing = true;
  lastFetchFailed = false;
  requestUpdateAndWait();
  if (!TodayHistoryClient::syncToday(true)) {
    lastFetchFailed = true;
  }
  TodayHistoryClient::loadCached(todayHistory);
  topIndex = std::min(topIndex, std::max(0, static_cast<int>(todayHistory.events.size()) - 1));
  syncing = false;
  requestUpdate();
}

int TodayHistoryActivity::visibleItemCount() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2 + 38;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  int y = top;
  int visible = 0;
  for (int i = topIndex; i < static_cast<int>(todayHistory.events.size()); ++i) {
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

int TodayHistoryActivity::itemHeight(const int index, const int contentWidth) const {
  if (index < 0 || index >= static_cast<int>(todayHistory.events.size())) {
    return 0;
  }
  char titleLine[128];
  snprintf(titleLine, sizeof(titleLine), "%s  %s", todayHistory.events[index].year,
           todayHistory.events[index].title.c_str());
  const int textWidth = contentWidth - TODAY_TEXT_LEFT_OFFSET;
  const auto titleLines = renderer.wrappedText(UI_10_FONT_ID, titleLine, textWidth, TODAY_TITLE_MAX_LINES,
                                               EpdFontFamily::BOLD);
  const auto descLines =
      renderer.wrappedText(SMALL_FONT_ID, todayHistory.events[index].desc.c_str(), textWidth, TODAY_DESC_MAX_LINES);
  const int titleLineHeight = renderer.getLineHeight(UI_10_FONT_ID) + 3;
  const int descLineHeight = renderer.getLineHeight(SMALL_FONT_ID) + 2;
  return std::max(titleLineHeight + TODAY_ITEM_GAP,
                  static_cast<int>(titleLines.size()) * titleLineHeight +
	                      static_cast<int>(descLines.size()) * descLineHeight + TODAY_ITEM_GAP);
}

int TodayHistoryActivity::visibleEndIndex() const {
  if (todayHistory.events.empty()) {
    return -1;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int listTop = top + 38;
  const int listBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  int y = listTop;
  int end = topIndex;
  for (int i = topIndex; i < static_cast<int>(todayHistory.events.size()); ++i) {
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

void TodayHistoryActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    refreshTodayHistory();
    return;
  }

  const int maxTop = std::max(0, static_cast<int>(todayHistory.events.size()) - 1);
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

void TodayHistoryActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentX = metrics.contentSidePadding;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TODAY_HISTORY));

  const char* status = syncing ? tr(STR_LOADING) : (lastFetchFailed ? tr(STR_TODAY_HISTORY_EMPTY) : todayHistory.update);
  char rangeText[20] = "";
  if (todayHistory.hasAny) {
    const int end = visibleEndIndex();
    snprintf(rangeText, sizeof(rangeText), "%d-%d/%d", topIndex + 1, end + 1,
             static_cast<int>(todayHistory.events.size()));
  }
  const int rangeWidth = rangeText[0] != '\0' ? renderer.getTextWidth(SMALL_FONT_ID, rangeText) : 0;
  const int statusMaxWidth = contentW - rangeWidth - (rangeWidth > 0 ? 12 : 0);
  const std::string safeStatus = renderer.truncatedText(UI_10_FONT_ID, status, std::max(40, statusMaxWidth));
  renderer.drawText(UI_10_FONT_ID, contentX, top, safeStatus.c_str());
  if (rangeText[0] != '\0') {
    renderer.drawText(SMALL_FONT_ID, contentX + contentW - rangeWidth, top, rangeText);
  }

  const int listTop = top + 38;
  const int listBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  int y = listTop;
  if (!todayHistory.hasAny) {
    renderer.drawText(UI_10_FONT_ID, contentX, y, tr(STR_TODAY_HISTORY_EMPTY));
  } else {
    for (int i = topIndex; i < static_cast<int>(todayHistory.events.size()); ++i) {
      const int neededHeight = itemHeight(i, contentW);
      if (i > topIndex && y + neededHeight > listBottom) {
        break;
      }
      char prefix[16];
      snprintf(prefix, sizeof(prefix), "%d.", i + 1);
      renderer.drawText(SMALL_FONT_ID, contentX, y + 2, prefix, true, EpdFontFamily::BOLD);
      const int textX = contentX + TODAY_TEXT_LEFT_OFFSET;

      char titleLine[128];
      snprintf(titleLine, sizeof(titleLine), "%s  %s", todayHistory.events[i].year, todayHistory.events[i].title.c_str());
      const auto titleLines = renderer.wrappedText(UI_10_FONT_ID, titleLine, contentW - TODAY_TEXT_LEFT_OFFSET,
                                                   TODAY_TITLE_MAX_LINES, EpdFontFamily::BOLD);
      int lineY = y;
      for (const auto& line : titleLines) {
        if (lineY + renderer.getLineHeight(UI_10_FONT_ID) > listBottom) {
          break;
        }
        renderer.drawText(UI_10_FONT_ID, textX, lineY, line.c_str(), true, EpdFontFamily::BOLD);
        lineY += renderer.getLineHeight(UI_10_FONT_ID) + 3;
      }

      const auto lines =
          renderer.wrappedText(SMALL_FONT_ID, todayHistory.events[i].desc.c_str(), contentW - TODAY_TEXT_LEFT_OFFSET,
                               TODAY_DESC_MAX_LINES);
      for (const auto& line : lines) {
        if (lineY + renderer.getLineHeight(SMALL_FONT_ID) > listBottom) {
          break;
        }
        renderer.drawText(SMALL_FONT_ID, textX, lineY, line.c_str());
        lineY += renderer.getLineHeight(SMALL_FONT_ID) + 2;
      }
      y += neededHeight;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

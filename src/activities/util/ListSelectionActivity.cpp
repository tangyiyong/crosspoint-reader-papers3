#include "ListSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <utility>

#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "components/UITheme.h"

ListSelectionActivity::ListSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title,
                                             std::vector<std::string> items, const int initialIndex)
    : Activity("ListSelection", renderer, mappedInput),
      title(std::move(title)),
      items(std::move(items)),
      selectedIndex(initialIndex) {}

void ListSelectionActivity::onEnter() {
  Activity::onEnter();
  if (!items.empty()) {
    selectedIndex = std::clamp(selectedIndex, 0, static_cast<int>(items.size()) - 1);
  } else {
    selectedIndex = 0;
  }
  requestUpdate();
}

Rect ListSelectionActivity::listRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  return Rect{0, contentTop, pageWidth, contentHeight};
}

void ListSelectionActivity::loop() {
  const Rect rect = listRect();
  const int itemCount = static_cast<int>(items.size());
  const int pageItems = UITheme::getInstance().getListItemsPerPage(rect, false);

  if (mappedInput.wasContentSwipedUp() || mappedInput.wasContentSwipedDown()) {
    if (pageItems > 0 && itemCount > pageItems) {
      selectedIndex = mappedInput.wasContentSwipedUp()
                          ? ButtonNavigator::nextPageIndex(selectedIndex, itemCount, pageItems)
                          : ButtonNavigator::previousPageIndex(selectedIndex, itemCount, pageItems);
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasContentTapped()) {
    const int tapped = UITheme::getInstance().hitTestListItem(rect, itemCount, selectedIndex, false,
                                                              mappedInput.getTouchX(), mappedInput.getTouchY());
    if (tapped >= 0) {
      selectedIndex = tapped;
      ActivityResult result;
      result.isCancelled = false;
      result.data = MenuResult{selectedIndex};
      setResult(std::move(result));
      finish();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    ActivityResult result;
    result.isCancelled = false;
    result.data = MenuResult{selectedIndex};
    setResult(std::move(result));
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
    requestUpdate();
  });
}

void ListSelectionActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title.c_str());
  GUI.drawList(
      renderer, listRect(), static_cast<int>(items.size()), selectedIndex,
      [this](int index) { return index >= 0 && index < static_cast<int>(items.size()) ? items[index] : ""; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

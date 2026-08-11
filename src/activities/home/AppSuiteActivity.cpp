#include "AppSuiteActivity.h"

#include <GfxRenderer.h>

#include "AppPlaceholderActivity.h"
#include "ClockCalendarActivity.h"
#include "MappedInputManager.h"
#include "WoodenFishActivity.h"
#include "activities/settings/OtaUpdateActivity.h"
#include "components/UITheme.h"

void AppSuiteActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

StrId AppSuiteActivity::labelFor(const BuiltInApp app) const {
  switch (app) {
    case BuiltInApp::Reader:
      return StrId::STR_APP_READER;
    case BuiltInApp::RecentBooks:
      return StrId::STR_MENU_RECENT_BOOKS;
    case BuiltInApp::Gallery:
      return StrId::STR_APP_GALLERY;
    case BuiltInApp::Notepad:
      return StrId::STR_APP_NOTEPAD;
    case BuiltInApp::Drawing:
      return StrId::STR_APP_DRAWING;
    case BuiltInApp::Music:
      return StrId::STR_APP_MUSIC;
    case BuiltInApp::Weather:
      return StrId::STR_APP_WEATHER;
    case BuiltInApp::ClockCalendar:
      return StrId::STR_APP_CLOCK_CALENDAR;
    case BuiltInApp::WoodenFish:
      return StrId::STR_APP_WOODEN_FISH;
    case BuiltInApp::FileManager:
      return StrId::STR_APP_FILE_MANAGER;
    case BuiltInApp::FileTransfer:
      return StrId::STR_FILE_TRANSFER;
    case BuiltInApp::Update:
      return StrId::STR_APP_UPDATE;
    case BuiltInApp::Settings:
      return StrId::STR_SETTINGS_TITLE;
    case BuiltInApp::Count:
    default:
      return StrId::STR_NONE_OPT;
  }
}

StrId AppSuiteActivity::valueFor(const BuiltInApp app) const {
  switch (app) {
    case BuiltInApp::Reader:
    case BuiltInApp::RecentBooks:
    case BuiltInApp::ClockCalendar:
    case BuiltInApp::WoodenFish:
    case BuiltInApp::FileManager:
    case BuiltInApp::FileTransfer:
    case BuiltInApp::Update:
    case BuiltInApp::Settings:
      return StrId::STR_APP_READY;
    case BuiltInApp::Gallery:
    case BuiltInApp::Notepad:
    case BuiltInApp::Drawing:
    case BuiltInApp::Weather:
      return StrId::STR_APP_STAGED;
    case BuiltInApp::Music:
      return StrId::STR_APP_NEEDS_HARDWARE;
    case BuiltInApp::Count:
    default:
      return StrId::STR_NONE_OPT;
  }
}

void AppSuiteActivity::openPlaceholder(const StrId title, const StrId message) {
  startActivityForResult(std::make_unique<AppPlaceholderActivity>(renderer, mappedInput, title, message), nullptr);
}

void AppSuiteActivity::activateSelected() {
  switch (itemAt(selectedIndex)) {
    case BuiltInApp::Reader:
      activityManager.goToFileBrowser();
      break;
    case BuiltInApp::RecentBooks:
      activityManager.goToRecentBooks();
      break;
    case BuiltInApp::Gallery:
      activityManager.goToFileBrowser("/pictures");
      break;
    case BuiltInApp::Notepad:
      openPlaceholder(StrId::STR_APP_NOTEPAD, StrId::STR_APP_NOTEPAD_PLACEHOLDER);
      break;
    case BuiltInApp::Drawing:
      openPlaceholder(StrId::STR_APP_DRAWING, StrId::STR_APP_DRAWING_PLACEHOLDER);
      break;
    case BuiltInApp::Music:
      openPlaceholder(StrId::STR_APP_MUSIC, StrId::STR_APP_MUSIC_PLACEHOLDER);
      break;
    case BuiltInApp::Weather:
      openPlaceholder(StrId::STR_APP_WEATHER, StrId::STR_APP_WEATHER_PLACEHOLDER);
      break;
    case BuiltInApp::ClockCalendar:
      startActivityForResult(std::make_unique<ClockCalendarActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::WoodenFish:
      startActivityForResult(std::make_unique<WoodenFishActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::FileManager:
      activityManager.goToFileBrowser();
      break;
    case BuiltInApp::FileTransfer:
      activityManager.goToFileTransfer();
      break;
    case BuiltInApp::Update:
      startActivityForResult(std::make_unique<OtaUpdateActivity>(renderer, mappedInput), nullptr);
      break;
    case BuiltInApp::Settings:
      activityManager.goToSettings();
      break;
    case BuiltInApp::Count:
      break;
  }
}

void AppSuiteActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
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

  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    activityManager.goHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
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
}

void AppSuiteActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_SUITE));
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount(), selectedIndex,
      [this](const int index) { return std::string(I18N.get(labelFor(itemAt(index)))); }, nullptr, nullptr,
      [this](const int index) { return std::string(I18N.get(valueFor(itemAt(index)))); });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

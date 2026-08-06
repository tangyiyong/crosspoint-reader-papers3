#include "OpdsSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

int OpdsSettingsActivity::getMenuItemCount() const {
  return isNewServer ? BASE_ITEMS : BASE_ITEMS + 1;
}

void OpdsSettingsActivity::onEnter() {
  Activity::onEnter();

  selectedIndex = 0;
  isNewServer = serverIndex < 0;
  showSaveError = false;

  if (!isNewServer) {
    const auto* server = OPDS_STORE.getServer(static_cast<size_t>(serverIndex));
    if (server) {
      editServer = *server;
    } else {
      isNewServer = true;
      serverIndex = -1;
    }
  }

  requestUpdate();
}

void OpdsSettingsActivity::onExit() { Activity::onExit(); }

void OpdsSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  const int menuItems = getMenuItemCount();
  buttonNavigator.onNext([this, menuItems] {
    selectedIndex = (selectedIndex + 1) % static_cast<size_t>(menuItems);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuItems] {
    selectedIndex = (selectedIndex + static_cast<size_t>(menuItems) - 1) % static_cast<size_t>(menuItems);
    requestUpdate();
  });
}

bool OpdsSettingsActivity::saveServer() {
  bool success = false;

  if (isNewServer) {
    success = OPDS_STORE.addServer(editServer);
    if (success) {
      isNewServer = false;
      serverIndex = static_cast<int>(OPDS_STORE.getCount()) - 1;
    } else {
      LOG_ERR("OPS", "Failed to add OPDS server");
    }
  } else {
    success = OPDS_STORE.updateServer(static_cast<size_t>(serverIndex), editServer);
    if (!success) {
      LOG_ERR("OPS", "Failed to update OPDS server at index %d", serverIndex);
    }
  }

  showSaveError = !success;
  if (showSaveError) {
    requestUpdate();
  }
  return success;
}

void OpdsSettingsActivity::handleSelection() {
  if (selectedIndex == 0) {
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_SERVER_NAME),
                                                                   editServer.name, 63, false),
                           [this](const ActivityResult& result) {
                             if (!result.isCancelled) {
                               const auto& kb = std::get<KeyboardResult>(result.data);
                               editServer.name = kb.text;
                               saveServer();
                               requestUpdate();
                             }
                           });
  } else if (selectedIndex == 1) {
    const std::string prefillUrl = editServer.url.empty() ? "https://" : editServer.url;
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_OPDS_SERVER_URL),
                                                                   prefillUrl, 127, false),
                           [this](const ActivityResult& result) {
                             if (!result.isCancelled) {
                               const auto& kb = std::get<KeyboardResult>(result.data);
                               editServer.url = (kb.text == "https://" || kb.text == "http://") ? "" : kb.text;
                               saveServer();
                               requestUpdate();
                             }
                           });
  } else if (selectedIndex == 2) {
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_USERNAME),
                                                                   editServer.username, 63, false),
                           [this](const ActivityResult& result) {
                             if (!result.isCancelled) {
                               const auto& kb = std::get<KeyboardResult>(result.data);
                               editServer.username = kb.text;
                               saveServer();
                               requestUpdate();
                             }
                           });
  } else if (selectedIndex == 3) {
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_PASSWORD),
                                                                   editServer.password, 63, true),
                           [this](const ActivityResult& result) {
                             if (!result.isCancelled) {
                               const auto& kb = std::get<KeyboardResult>(result.data);
                               editServer.password = kb.text;
                               saveServer();
                               requestUpdate();
                             }
                           });
  } else if (selectedIndex == 4 && !isNewServer) {
    if (!OPDS_STORE.removeServer(static_cast<size_t>(serverIndex))) {
      LOG_ERR("OPS", "Failed to remove OPDS server at index %d", serverIndex);
      showSaveError = true;
      requestUpdate();
      return;
    }
    finish();
  }
}

void OpdsSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const char* header = isNewServer ? tr(STR_ADD_SERVER) : tr(STR_OPDS_BROWSER);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);
  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    tr(STR_CALIBRE_URL_HINT));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + metrics.tabBarHeight;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  static constexpr StrId fieldNames[BASE_ITEMS] = {StrId::STR_SERVER_NAME, StrId::STR_OPDS_SERVER_URL,
                                                   StrId::STR_USERNAME, StrId::STR_PASSWORD};

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, getMenuItemCount(), static_cast<int>(selectedIndex),
      [](int index) {
        if (index < BASE_ITEMS) {
          return std::string(I18N.get(fieldNames[index]));
        }
        return std::string(tr(STR_DELETE_SERVER));
      },
      nullptr, nullptr,
      [this](int index) {
        if (index == 0) {
          return editServer.name.empty() ? std::string(tr(STR_NOT_SET)) : editServer.name;
        }
        if (index == 1) {
          return editServer.url.empty() ? std::string(tr(STR_NOT_SET)) : editServer.url;
        }
        if (index == 2) {
          return editServer.username.empty() ? std::string(tr(STR_NOT_SET)) : editServer.username;
        }
        if (index == 3) {
          return editServer.password.empty() ? std::string(tr(STR_NOT_SET)) : std::string("******");
        }
        return std::string("");
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (showSaveError) {
    GUI.drawPopup(renderer, tr(STR_ERROR_GENERAL_FAILURE));
  }

  renderer.displayBuffer();
}

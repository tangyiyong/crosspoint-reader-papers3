#pragma once

#include "OpdsServerStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class OpdsSettingsActivity final : public Activity {
 public:
  explicit OpdsSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int serverIndex = -1)
      : Activity("OpdsSettings", renderer, mappedInput), serverIndex(serverIndex) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int BASE_ITEMS = 4;

  ButtonNavigator buttonNavigator;
  size_t selectedIndex = 0;
  int serverIndex;
  OpdsServer editServer;
  bool isNewServer = false;
  bool showSaveError = false;

  int getMenuItemCount() const;
  void handleSelection();
  bool saveServer();
};

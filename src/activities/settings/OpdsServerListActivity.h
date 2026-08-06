#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class OpdsServerListActivity final : public Activity {
 public:
  explicit OpdsServerListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool pickerMode = false)
      : Activity("OpdsServerList", renderer, mappedInput), pickerMode(pickerMode) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool pickerMode = false;

  int getItemCount() const;
  void handleSelection();
};

#pragma once

#include "activities/Activity.h"

class FontSelectActivity final : public Activity {
 public:
  explicit FontSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FontSelect", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int selectedIndex = 0;
  int totalItems = 1;

  void applySelection();
};

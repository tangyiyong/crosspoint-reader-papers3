#pragma once

#include "activities/Activity.h"

class WoodenFishActivity final : public Activity {
  uint32_t count = 0;

 public:
  explicit WoodenFishActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("WoodenFish", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

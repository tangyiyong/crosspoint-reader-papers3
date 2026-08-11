#pragma once

#include "activities/Activity.h"

class UsbMassStorageActivity final : public Activity {
 public:
  explicit UsbMassStorageActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("UsbMassStorage", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  bool preventAutoSleep() override { return true; }
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  bool started = false;
};

#pragma once

#include "activities/Activity.h"

class AlarmActivity final : public Activity {
  uint8_t delayMinutes = 10;
  bool armed = false;
  unsigned long dueAt = 0;

  void adjust(int delta);
  long remainingSeconds() const;

 public:
  explicit AlarmActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Alarm", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return armed; }
};

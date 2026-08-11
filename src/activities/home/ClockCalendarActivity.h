#pragma once

#include "activities/Activity.h"

class ClockCalendarActivity final : public Activity {
 public:
  explicit ClockCalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ClockCalendar", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

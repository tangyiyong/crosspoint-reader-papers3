#pragma once

#include "activities/Activity.h"

class PomodoroActivity final : public Activity {
  static constexpr unsigned long WORK_MS = 25UL * 60UL * 1000UL;
  static constexpr unsigned long BREAK_MS = 5UL * 60UL * 1000UL;
  bool running = false;
  bool breakMode = false;
  unsigned long startedAt = 0;
  unsigned long durationMs = WORK_MS;

  unsigned long remainingMs() const;
  void toggle();
  void reset();

 public:
  explicit PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Pomodoro", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return running; }
};

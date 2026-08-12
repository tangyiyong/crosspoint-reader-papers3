#pragma once

#include "activities/Activity.h"

class ClockCalendarActivity final : public Activity {
 public:
  explicit ClockCalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ClockCalendar", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int displayYear = 0;
  int displayMonth = 0;
  int todayYear = 0;
  int todayMonth = 0;
  int todayDay = 0;
  int selectedDay = 0;
  bool showingDayDetail = false;
  bool monthSyncAttempted = false;
  bool monthSyncing = false;
  bool daySyncing = false;

  void changeMonth(int delta);
  void syncDisplayedMonthIfNeeded();
  void openDayDetail(int day);
  bool loadCurrentLocalDate();
  int hitTestDay(int touchX, int touchY) const;
  void drawMonthCalendar() const;
  void drawDayDetail() const;
};

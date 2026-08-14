#pragma once

#include "activities/Activity.h"
#include "today/TodayHistoryClient.h"
#include "util/ButtonNavigator.h"

class TodayHistoryActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  TodayHistoryInfo todayHistory;
  int topIndex = 0;
  bool syncing = false;
  bool lastFetchFailed = false;

  void refreshTodayHistory();
  int visibleItemCount() const;
  int itemHeight(int index, int contentWidth) const;
  int visibleEndIndex() const;

 public:
  explicit TodayHistoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TodayHistory", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

#pragma once

#include "activities/Activity.h"
#include "news/NewsDataClient.h"
#include "util/ButtonNavigator.h"

class HotNewsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  HotNewsInfo news;
  int topIndex = 0;
  bool syncing = false;
  bool lastFetchFailed = false;

  void refreshNews();
  int visibleItemCount() const;

 public:
  explicit HotNewsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("HotNews", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

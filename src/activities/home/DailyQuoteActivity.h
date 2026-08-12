#pragma once

#include "activities/Activity.h"
#include "quote/QuoteDataClient.h"

class DailyQuoteActivity final : public Activity {
  QuoteInfo quote;
  bool syncing = false;
  bool lastFetchFailed = false;

  void refreshQuote();

 public:
  explicit DailyQuoteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DailyQuote", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

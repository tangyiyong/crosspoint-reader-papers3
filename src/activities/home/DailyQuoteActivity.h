#pragma once

#include "activities/Activity.h"
#include "quote/QuoteDataClient.h"
#include "util/ButtonNavigator.h"

class DailyQuoteActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  QuoteInfo quote;
  int topLine = 0;
  bool syncing = false;
  bool lastFetchFailed = false;

  void refreshQuote();
  int visibleLineCount() const;
  int totalLineCount() const;

 public:
  explicit DailyQuoteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DailyQuote", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

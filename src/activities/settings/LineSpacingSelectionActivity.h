#pragma once

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "activities/ActivityResult.h"
#include "util/ButtonNavigator.h"

class LineSpacingSelectionActivity final : public Activity {
 public:
  explicit LineSpacingSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const int initialValue)
      : Activity("LineSpacingSelection", renderer, mappedInput), value(initialValue) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int value = CrossPointSettings::LINE_SPACING_DEFAULT;
  ButtonNavigator buttonNavigator;

  void adjustValue(int delta);
};

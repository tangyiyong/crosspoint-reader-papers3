#pragma once

#include <I18n.h>

#include "activities/Activity.h"

class AppPlaceholderActivity final : public Activity {
  StrId title;
  StrId message;

 public:
  AppPlaceholderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const StrId title, const StrId message)
      : Activity("AppPlaceholder", renderer, mappedInput), title(title), message(message) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

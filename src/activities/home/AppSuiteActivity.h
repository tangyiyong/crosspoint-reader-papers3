#pragma once

#include <I18n.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class AppSuiteActivity final : public Activity {
  enum class BuiltInApp : uint8_t {
    Reader,
    RecentBooks,
    Gallery,
    Notepad,
    Drawing,
    Music,
    Weather,
    ClockCalendar,
    WoodenFish,
    FileManager,
    FileTransfer,
    Update,
    Settings,
    Count,
  };

  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  static constexpr int itemCount() { return static_cast<int>(BuiltInApp::Count); }
  static BuiltInApp itemAt(int index) { return static_cast<BuiltInApp>(index); }

  StrId labelFor(BuiltInApp app) const;
  StrId valueFor(BuiltInApp app) const;
  void activateSelected();
  void openPlaceholder(StrId title, StrId message);

 public:
  explicit AppSuiteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("AppSuite", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

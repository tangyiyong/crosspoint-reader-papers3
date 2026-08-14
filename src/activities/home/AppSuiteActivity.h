#pragma once

#include <I18n.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct Rect;

class AppSuiteActivity final : public Activity {
  enum class BuiltInApp : uint8_t {
    Reader,
    RecentBooks,
    Gallery,
    Weather,
    DailyQuote,
    TodayHistory,
    HotNews,
    Notepad,
    Pomodoro,
    Alarm,
    ClockCalendar,
    WoodenFish,
    FileManager,
    FileTransfer,
    Update,
    Count,
  };

  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  static constexpr int itemCount() { return static_cast<int>(BuiltInApp::Count); }
  static BuiltInApp itemAt(int index) { return static_cast<BuiltInApp>(index); }
  static constexpr int gridCols() { return 4; }
  static constexpr int gridRows() { return 5; }
  static constexpr int gridCellHeight() { return 128; }
  static constexpr int gridItemsPerPage() { return gridCols() * gridRows(); }

  StrId labelFor(BuiltInApp app) const;
  StrId valueFor(BuiltInApp app) const;
  void activateSelected();
  int hitTestGrid(Rect rect, int touchX, int touchY) const;
  void drawAppIcon(BuiltInApp app, int centerX, int topY, int size, bool selected) const;

 public:
  explicit AppSuiteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("AppSuite", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

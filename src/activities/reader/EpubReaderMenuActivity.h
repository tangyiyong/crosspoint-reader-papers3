#pragma once
#include <Epub.h>
#include <I18n.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class EpubReaderMenuActivity final : public Activity {
 public:
  // Menu actions available from the reader menu.
  enum class MenuAction {
    SELECT_CHAPTER,
    FOOTNOTES,
    GO_TO_PERCENT,
    AUTO_PAGE_TURN,
    ROTATE_SCREEN,
    COLOR_MODE,
    SCREENSHOT,
    DISPLAY_QR,
    GO_HOME,
    SYNC,
    DELETE_CACHE
  };
  using ActionMask = uint16_t;
  static constexpr ActionMask actionMask(MenuAction action) {
    return static_cast<ActionMask>(1U << static_cast<uint8_t>(action));
  }
  static constexpr ActionMask ALL_ACTIONS = 0x07FF;

  explicit EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                  const int currentPage, const int totalPages, const int bookProgressPercent,
                                  const uint8_t currentOrientation, const bool hasFootnotes,
                                  ActionMask enabledActions = ALL_ACTIONS);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return false; }
  bool supportsLandscape() const override { return true; }

 private:
  struct MenuItem {
    MenuAction action;
    StrId labelId;
  };

  static std::vector<MenuItem> buildMenuItems(bool hasFootnotes, ActionMask enabledActions);

  // Fixed menu layout
  const std::vector<MenuItem> menuItems;

  int selectedIndex = 0;

  ButtonNavigator buttonNavigator;
  std::string title = "Reader Menu";
  uint8_t pendingOrientation = 0;
  uint8_t selectedPageTurnOption = 0;
#if CROSSPOINT_PAPERS3
  const std::vector<StrId> orientationLabels = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CCW};
#else
  const std::vector<StrId> orientationLabels = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW, StrId::STR_INVERTED,
                                                StrId::STR_LANDSCAPE_CCW};
#endif
  const std::vector<const char*> pageTurnLabels = {I18N.get(StrId::STR_STATE_OFF), "1", "3", "6", "12"};
  int currentPage = 0;
  int totalPages = 0;
  int bookProgressPercent = 0;

  std::string getMenuItemValue(MenuAction action) const;
};

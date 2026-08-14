#pragma once

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Lyra theme metrics (zero runtime cost)
namespace LyraMetrics {
constexpr ThemeMetrics values = {.batteryWidth = 16,
                                 .batteryHeight = 12,
	                                 .topPadding =
#if CROSSPOINT_PAPERS3
	                                     0,
#else
	                                     2,
#endif
	                                 .batteryBarHeight =
#if CROSSPOINT_PAPERS3
		                                     30,
#else
	                                     40,
#endif
	                                 .headerHeight =
#if CROSSPOINT_PAPERS3
		                                     58,
#else
	                                     64,
#endif
                                 .verticalSpacing = 8,
                                 .contentSidePadding = 20,
#if CROSSPOINT_PAPERS3
                                 .listRowHeight = 60,
                                 .listWithSubtitleRowHeight = 70,
                                 .menuRowHeight = 60,
                                 .menuSpacing = 4,
#else
                                 .listRowHeight = 40,
                                 .listWithSubtitleRowHeight = 60,
                                 .menuRowHeight = 64,
                                 .menuSpacing = 8,
#endif
                                 .tabSpacing = 8,
#if CROSSPOINT_PAPERS3
                                 .tabBarHeight = 56,
#else
                                 .tabBarHeight = 40,
#endif
                                 .scrollBarWidth = 4,
                                 .scrollBarRightOffset = 5,
	                                 .homeTopPadding =
#if CROSSPOINT_PAPERS3
	                                     34,
#else
	                                     42,
#endif
#if CROSSPOINT_PAPERS3
                                 .homeCoverHeight = 305,
                                 .homeCoverTileHeight = 327,
#else
                                 .homeCoverHeight = 226,
                                 .homeCoverTileHeight = 242,
#endif
                                 .homeRecentBooksCount = 1,
#if CROSSPOINT_PAPERS3
	                                 .buttonHintsHeight = 70,
                                 .sideButtonHintsWidth = 0,
#else
                                 .buttonHintsHeight = 40,
                                 .sideButtonHintsWidth = 30,
#endif
                                 .progressBarHeight = 16,
                                 .progressBarMarginTop = 1,
                                 .statusBarHorizontalMargin = 5,
#if CROSSPOINT_PAPERS3
                                 .statusBarVerticalMargin = 18,
#else
                                 .statusBarVerticalMargin = 19,
#endif
                                 .keyboardKeyWidth = 31,
                                 .keyboardKeyHeight = 50,
                                 .keyboardKeySpacing = 0,
                                 .keyboardBottomAligned = true,
                                 .keyboardCenteredText = true};
}  // namespace LyraMetrics

class LyraTheme : public BaseTheme {
 public:
  // Component drawing methods
  //   void drawProgressBar(const GfxRenderer& renderer, Rect rect, size_t current, size_t total) override;
  void drawBatteryLeft(const GfxRenderer& renderer, Rect rect, bool showPercentage = true) const override;
  void drawBatteryRight(const GfxRenderer& renderer, Rect rect, bool showPercentage = true) const override;
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle) const override;
  void drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                     const char* rightLabel = nullptr) const override;
  void drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                  bool selected) const override;
  void drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                const std::function<std::string(int index)>& rowTitle,
                const std::function<std::string(int index)>& rowSubtitle,
                const std::function<UIIcon(int index)>& rowIcon, const std::function<std::string(int index)>& rowValue,
                bool highlightValue) const override;
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const override;
  void drawSideButtonHints(const GfxRenderer& renderer, const char* topBtn, const char* bottomBtn) const override;
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           std::function<bool()> storeCoverBuffer) const override;
  void drawEmptyRecents(const GfxRenderer& renderer, const Rect rect) const;
  Rect drawPopup(const GfxRenderer& renderer, const char* message) const override;
  void fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, const int progress) const override;
  void drawTextField(const GfxRenderer& renderer, Rect rect, const int textWidth) const override;
  void drawKeyboardKey(const GfxRenderer& renderer, Rect rect, const char* label, const bool isSelected) const override;
};

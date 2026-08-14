

#pragma once

#include "components/themes/lyra/LyraTheme.h"

class GfxRenderer;

// Lyra theme metrics (zero runtime cost)
namespace Lyra3CoversMetrics {
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
                                 .homeCoverTileHeight = 405,
                                 .homeRecentBooksCount = 3,
#else
                                 .homeCoverHeight = 226,
                                 .homeCoverTileHeight = 300,
                                 .homeRecentBooksCount = 3,
#endif
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
                                 .statusBarVerticalMargin = 20,
#else
                                 .statusBarVerticalMargin = 19,
#endif
                                 .keyboardKeyWidth = 31,
                                 .keyboardKeyHeight = 50,
                                 .keyboardKeySpacing = 0,
                                 .keyboardBottomAligned = true,
                                 .keyboardCenteredText = true};
}  // namespace Lyra3CoversMetrics

class Lyra3CoversTheme : public LyraTheme {
 public:
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           std::function<bool()> storeCoverBuffer) const override;
};

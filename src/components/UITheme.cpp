#include "UITheme.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <Logging.h>

#include <memory>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/themes/BaseTheme.h"
#include "components/themes/lyra/Lyra3CoversTheme.h"
#include "components/themes/lyra/LyraTheme.h"

namespace {
constexpr int SKIP_PAGE_MS = 700;
}  // namespace

UITheme UITheme::instance;

UITheme::UITheme() {
  auto themeType = static_cast<CrossPointSettings::UI_THEME>(SETTINGS.uiTheme);
  setTheme(themeType);
}

void UITheme::reload() {
  auto themeType = static_cast<CrossPointSettings::UI_THEME>(SETTINGS.uiTheme);
  setTheme(themeType);
}

void UITheme::setTheme(CrossPointSettings::UI_THEME type) {
  switch (type) {
    case CrossPointSettings::UI_THEME::CLASSIC:
      LOG_DBG("UI", "Using Classic theme");
      currentTheme = std::make_unique<BaseTheme>();
      currentMetrics = &BaseMetrics::values;
      break;
    case CrossPointSettings::UI_THEME::LYRA:
      LOG_DBG("UI", "Using Lyra theme");
      currentTheme = std::make_unique<LyraTheme>();
      currentMetrics = &LyraMetrics::values;
      break;
    case CrossPointSettings::UI_THEME::LYRA_3_COVERS:
      LOG_DBG("UI", "Using Lyra 3 Covers theme");
      currentTheme = std::make_unique<Lyra3CoversTheme>();
      currentMetrics = &Lyra3CoversMetrics::values;
      break;
  }
}

int UITheme::getNumberOfItemsPerPage(const GfxRenderer& renderer, bool hasHeader, bool hasTabBar, bool hasButtonHints,
                                     bool hasSubtitle, int extraReservedHeight) {
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  int reservedHeight = metrics.topPadding;
  if (hasHeader) {
    reservedHeight += metrics.headerHeight + metrics.verticalSpacing;
  }
  if (hasTabBar) {
    reservedHeight += metrics.tabBarHeight;
  }
  if (hasButtonHints) {
    reservedHeight += metrics.verticalSpacing + metrics.buttonHintsHeight;
  }
  reservedHeight += extraReservedHeight;
  const int availableHeight = renderer.getScreenHeight() - reservedHeight;
  int rowHeight = hasSubtitle ? metrics.listWithSubtitleRowHeight : metrics.listRowHeight;
  return availableHeight / rowHeight;
}

int UITheme::getListItemsPerPage(const Rect rect, const bool hasSubtitle) const {
  const int rowHeight = hasSubtitle ? currentMetrics->listWithSubtitleRowHeight : currentMetrics->listRowHeight;
  return rowHeight > 0 ? rect.height / rowHeight : 0;
}

int UITheme::hitTestListItem(const Rect rect, const int itemCount, const int selectedIndex, const bool hasSubtitle,
                             const int touchX, const int touchY) const {
  if (itemCount <= 0 || touchX < rect.x || touchX >= rect.x + rect.width || touchY < rect.y ||
      touchY >= rect.y + rect.height) {
    return -1;
  }

  const int rowHeight = hasSubtitle ? currentMetrics->listWithSubtitleRowHeight : currentMetrics->listRowHeight;
  const int pageItems = getListItemsPerPage(rect, hasSubtitle);
  if (pageItems <= 0) {
    return -1;
  }

  const int row = (touchY - rect.y) / rowHeight;
  if (row < 0 || row >= pageItems) {
    return -1;
  }

  const int normalizedSelectedIndex = selectedIndex >= 0 ? selectedIndex : 0;
  const int pageStartIndex = (normalizedSelectedIndex / pageItems) * pageItems;
  const int itemIndex = pageStartIndex + row;
  return itemIndex < itemCount ? itemIndex : -1;
}

int UITheme::hitTestButtonMenu(const Rect rect, const int buttonCount, const int touchX, const int touchY) const {
  if (buttonCount <= 0 || touchX < rect.x || touchX >= rect.x + rect.width || touchY < rect.y ||
      touchY >= rect.y + rect.height) {
    return -1;
  }

  const int rowStride = currentMetrics->menuRowHeight + currentMetrics->menuSpacing;
  if (rowStride <= 0) {
    return -1;
  }

  const int localY = touchY - rect.y - currentMetrics->verticalSpacing;
  if (localY < 0) {
    return -1;
  }

  const int index = localY / rowStride;
  const int rowOffset = localY % rowStride;
  if (index < 0 || index >= buttonCount || rowOffset >= currentMetrics->menuRowHeight) {
    return -1;
  }

  const int contentLeft = rect.x + currentMetrics->contentSidePadding;
  const int contentRight = rect.x + rect.width - currentMetrics->contentSidePadding;
  if (touchX < contentLeft || touchX >= contentRight) {
    return -1;
  }

  return index;
}

std::string UITheme::getCoverThumbPath(std::string coverBmpPath, int coverHeight) {
  size_t pos = coverBmpPath.find("[HEIGHT]", 0);
  if (pos != std::string::npos) {
    coverBmpPath.replace(pos, 8, std::to_string(coverHeight));
  }
  return coverBmpPath;
}

UIIcon UITheme::getFileIcon(const std::string& filename) {
  if (filename.back() == '/') {
    return Folder;
  }
  if (FsHelpers::hasEpubExtension(filename) || FsHelpers::hasXtcExtension(filename)) {
    return Book;
  }
  if (FsHelpers::hasTxtExtension(filename) || FsHelpers::hasMarkdownExtension(filename)) {
    return Text;
  }
  if (FsHelpers::hasBmpExtension(filename)) {
    return Image;
  }
  return File;
}

int UITheme::getStatusBarHeight() {
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();

  // Add status bar margin
  const bool showStatusBar = SETTINGS.statusBarChapterPageCount || SETTINGS.statusBarBookProgressPercentage ||
                             SETTINGS.statusBarTitle != CrossPointSettings::STATUS_BAR_TITLE::HIDE_TITLE ||
                             SETTINGS.statusBarBattery;
  const bool showProgressBar =
      SETTINGS.statusBarProgressBar != CrossPointSettings::STATUS_BAR_PROGRESS_BAR::HIDE_PROGRESS;
  return (showStatusBar ? (metrics.statusBarVerticalMargin) : 0) +
         (showProgressBar ? (((SETTINGS.statusBarProgressBarThickness + 1) * 2) + metrics.progressBarMarginTop) : 0);
}

int UITheme::getProgressBarHeight() {
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  const bool showProgressBar =
      SETTINGS.statusBarProgressBar != CrossPointSettings::STATUS_BAR_PROGRESS_BAR::HIDE_PROGRESS;
  return (showProgressBar ? (((SETTINGS.statusBarProgressBarThickness + 1) * 2) + metrics.progressBarMarginTop) : 0);
}

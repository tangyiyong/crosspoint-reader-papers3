#pragma once
#include <functional>
#include <vector>

#include "../Activity.h"
#include "./FileBrowserActivity.h"
#include "today/TodayHistoryClient.h"
#include "util/ButtonNavigator.h"

struct RecentBook;
struct Rect;

class HomeActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  bool firstRenderDone = false;
  bool hasOpdsUrl = false;
  bool coverRendered = false;      // Track if cover has been rendered once
  bool coverBufferStored = false;  // Track if cover buffer is stored
  uint8_t* coverBuffer = nullptr;  // HomeActivity's own buffer for cover image
  TodayHistoryInfo todayHistory;
  int todayHistoryTopIndex = 0;
  bool todayHistorySyncAttempted = false;
  std::vector<RecentBook> recentBooks;
  void onSelectBook(const std::string& path);
  void onFileBrowserOpen();
  void onRecentsOpen();
  void onSettingsOpen();
  void onFileTransferOpen();
  void onAppSuiteOpen();
  void onOpdsBrowserOpen();
  void activateSelectedItem();

  int getMenuItemCount() const;
  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void loadRecentBooks(int maxBooks);
  void loadRecentCovers(int coverHeight);
  void loadTodayHistoryCache();
  void syncTodayHistoryIfNeeded();
  Rect todayHistoryRect(int pageWidth, int pageHeight) const;
  Rect homeMenuRect(int pageWidth, int pageHeight) const;
  int todayHistoryVisibleItems(Rect rect) const;
  void drawTodayHistory(Rect rect) const;
  int hitTestHomeMenu(Rect rect, int itemCount, int touchX, int touchY) const;
  void drawCompactHomeMenu(Rect rect, const std::vector<const char*>& menuItems);

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Home", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

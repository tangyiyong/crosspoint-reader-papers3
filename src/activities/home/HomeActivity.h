#pragma once
#include <functional>
#include <vector>

#include "../Activity.h"
#include "./FileBrowserActivity.h"
#include "quote/QuoteDataClient.h"
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
  QuoteInfo homeQuote;
  bool quoteSyncAttempted = false;
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
  void loadQuoteCache();
  void syncQuoteIfNeeded();
  Rect quoteRect(int pageWidth, int pageHeight, int menuItemCount) const;
  Rect homeMenuRect(int pageWidth, int pageHeight, int menuItemCount) const;
  void drawHomeQuote(Rect rect) const;
  int hitTestRecentBook(Rect rect, int touchX, int touchY) const;
  int hitTestHomeMenu(Rect rect, int itemCount, int touchX, int touchY) const;
  void drawHomeMenu(Rect rect, const std::vector<const char*>& menuItems);

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Home", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

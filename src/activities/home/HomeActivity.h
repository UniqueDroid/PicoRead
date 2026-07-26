#pragma once
#include <functional>
#include <vector>

#include "./FileBrowserActivity.h"
#include "HomeMenuLayoutStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct RecentBook;
struct Rect;

class HomeActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  bool firstRenderDone = false;
  bool hasOpdsServers = false;
  bool coverRendered = false;      // Track if cover has been rendered once
  bool coverBufferStored = false;  // Track if cover buffer is stored
  uint8_t* coverBuffer = nullptr;  // HomeActivity's own buffer for cover image
  size_t coverBufferSize = 0;      // Bytes allocated to coverBuffer
  // Logical rect last passed to drawRecentBookCover. The cover snapshot only
  // needs to cover this region, not the entire framebuffer, so we cache the
  // tile instead of all 48 KB. Set in render() before the call.
  int coverRectX = 0;
  int coverRectY = 0;
  int coverRectW = 0;
  int coverRectH = 0;
  std::vector<RecentBook> recentBooks;
  const HomeMenuItem initialMenuItem;

  // Convert HomeMenuItem to menu index (used in onEnter). Walks the user's
  // stored order (HomeMenuLayoutStore, which includes Settings - it's always
  // visible but can be moved like any other tile).
  static int menuItemToIndex(HomeMenuItem item, bool hasOpdsUrl) {
    int i = 0;
    for (const auto& entry : HOME_MENU_LAYOUT.getEntries()) {
      if (!entry.visible) continue;
      if (entry.item == HomeMenuItem::OPDS_BROWSER && !hasOpdsUrl) continue;
      if (entry.item == item) return i;
      ++i;
    }
    return 0;
  }

  // Convert menu index to HomeMenuItem (used in loop) - same store walk, inverse direction.
  static HomeMenuItem indexToMenuItem(int idx, bool hasOpdsUrl) {
    int i = 0;
    for (const auto& entry : HOME_MENU_LAYOUT.getEntries()) {
      if (!entry.visible) continue;
      if (entry.item == HomeMenuItem::OPDS_BROWSER && !hasOpdsUrl) continue;
      if (idx == i++) return entry.item;
    }
    return HomeMenuItem::NONE;
  }
  void onSelectBook(const std::string& path);
  void onFileBrowserOpen();
  void onRecentsOpen();
  void onAllBookmarksOpen();
  void onReadingStatsOpen();
  void onFlappyGameOpen();
  void onTetrisGameOpen();
  void onRssFeedsOpen();
  void onWikipediaOpen();
  void onGutenbergOpen();
  void onSettingsOpen();
  void onFileTransferOpen();
  void onOpdsBrowserOpen();

  int getMenuItemCount() const;
  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void loadRecentBooks(int maxBooks);
  void loadRecentCovers(int coverHeight);

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        HomeMenuItem initialMenuItemValue = HomeMenuItem::NONE)
      : Activity("Home", renderer, mappedInput), initialMenuItem(initialMenuItemValue) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

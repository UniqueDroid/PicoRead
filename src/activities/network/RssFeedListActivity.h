#pragma once
#include <functional>
#include <string>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Home-screen "RSS" tile: manage subscribed feeds and sync them for offline
// reading. Synced articles are written as plain .txt files under
// /.picoread/rss/<feedIndex>/ and read via the existing file browser + TXT
// reader - no dedicated article-reading UI needed.
class RssFeedListActivity final : public Activity {
  enum class State { List, Busy };

  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  State state = State::List;
  std::string busyMessage;
  bool shouldTearDownWifiOnExit = false;

  static int fixedRowCount() { return 3; }  // "Add Feed" + "Sync Now" + "Import from SD"
  int itemCount() const;

  // Runs action() immediately if WiFi is already connected, otherwise launches
  // WifiSelectionActivity first and runs action() only on a successful connect.
  void ensureWifiThen(const std::function<void()>& action);

  void startAddFeedFlow();
  void onUrlEntered(const std::string& url);
  void fetchAndAddFeed(const std::string& url);

  void startSyncFlow();
  void syncAllFeeds();
  bool syncOneFeed(size_t feedIndex);

  // Bulk-adds feeds from a JSON file dropped onto the SD card, no WiFi needed.
  void startImportFlow();

  void onSelectFeed(size_t feedIndex);

 public:
  explicit RssFeedListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RssFeedList", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

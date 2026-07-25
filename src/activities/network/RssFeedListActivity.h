#pragma once
#include <string>

#include "NetworkActivity.h"
#include "components/ScrollingListRow.h"
#include "util/ButtonNavigator.h"

// Home-screen "RSS" tile: manage subscribed feeds and sync them for offline
// reading. Synced articles are written as plain .txt files under
// /.picoread/rss/<feedIndex>/ and read via the existing file browser + TXT
// reader - no dedicated article-reading UI needed.
//
// Layout: subscribed feeds on top (or a "no feeds" placeholder), a thin
// separator, then the fixed actions (Add Feed / Sync Now / Import from SD /
// Manage Feeds) below.
class RssFeedListActivity final : public NetworkActivity {
  enum class State { List, Busy };

  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  State state = State::List;
  std::string busyMessage;
  int busyProgressPercent = -1;  // -1 = no progress bar, just the message

  // Marquee-scrolls the selected feed row's title when it's too long to fit -
  // shared mechanism, see ScrollingListRow.h.
  MarqueeScroller scroller;

  // 1 when there are no feeds yet (a non-actionable placeholder row), else one row
  // per feed.
  int feedRegionCount() const;
  static int actionRowCount() { return 4; }  // Add Feed, Sync Now, Import from SD, Manage Feeds
  int itemCount() const;

  void startAddFeedFlow();
  void onUrlEntered(const std::string& url);
  void fetchAndAddFeed(const std::string& url);

  void startSyncFlow();
  void syncAllFeeds();
  bool syncOneFeed(size_t feedIndex);

  // Bulk-adds feeds from a JSON file dropped onto the SD card, no WiFi needed.
  void startImportFlow();

  void startManageFeedsFlow();

  void onSelectFeed(size_t feedIndex);

 public:
  explicit RssFeedListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : NetworkActivity("RssFeedList", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

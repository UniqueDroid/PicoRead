#pragma once
#include <string>

#include "NetworkActivity.h"
#include "components/ScrollingListRow.h"
#include "util/ButtonNavigator.h"

// Home-screen "Wikipedia" tile: fetches the Wikimedia REST API's featured
// article of the day, most-read articles digest, on-this-day digest, and a
// handful of random articles via a single "Sync Now" action, saving all of
// them to fixed SD paths so they're readable offline afterward - selecting an
// entry just opens the cached file, it does not re-fetch. The featured-article
// and most-read entries ride on the same /feed/featured HTTP response, so
// adding most-read cost no extra network round-trip. Uses the system's
// configured UI language to pick the Wikipedia edition. Layout mirrors
// RssFeedListActivity: content entries in one region, a divider, then Sync
// Now below as its own action row.
class WikipediaActivity final : public NetworkActivity {
  enum class State { List, Busy };

  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  State state = State::List;
  std::string busyMessage;
  int busyProgressPercent = -1;  // -1 = no progress bar, just the message

  // Marquee-scrolls the selected row's title when it's too long to fit - same
  // shared mechanism as the RSS/Gutenberg lists, see ScrollingListRow.h.
  MarqueeScroller scroller;

  static constexpr int kRandomCount = 5;
  // Article of Day, Most Read, On This Day, Random x N
  static int contentItemCount() { return 3 + kRandomCount; }
  static int actionItemCount() { return 1; }  // Sync Now
  static int itemCount() { return contentItemCount() + actionItemCount(); }

  bool downloadFeatured();
  bool downloadOnThisDay();
  bool downloadRandomArticle(int index);
  void syncAll();
  void openTextOrPromptSync(const std::string& path);
  void promptSync();
  void openContentEntry(int index);
  std::string contentLabelFor(int index) const;

 public:
  explicit WikipediaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : NetworkActivity("Wikipedia", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

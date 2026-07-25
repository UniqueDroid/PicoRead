#pragma once
#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "GutenbergJsonParser.h"
#include "GutenbergPaths.h"
#include "components/ScrollingListRow.h"
#include "util/ButtonNavigator.h"

// Home-screen "Gutenberg" tile: browses a fixed handful of public-domain books via
// the Gutendex API (https://gutendex.com - a JSON index over Project Gutenberg) -
// a random pick plus the current top-5 most-downloaded books - and downloads each
// one's real EPUB (not a plain-text dump), so it reads with proper
// chapters/table-of-contents through the existing EpubReaderActivity, no new
// reading UI needed.
//
// Unlike WikipediaActivity's "sync everything, then read offline" model, this
// downloads one book at a time, on selection: "Sync Now" only fetches the popular
// list's *metadata* (titles + EPUB URLs, one small request), never the books
// themselves - a first version that downloaded all 6 books during sync turned out
// to take several minutes with no feedback in between and looked like a freeze.
// Selecting Random Book always re-fetches+downloads fresh (that's the point of
// "random"); selecting a Popular Book downloads just that one, once, and reuses
// the cached file after that.
class GutenbergActivity final : public Activity {
  enum class State { List, Busy };

  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  State state = State::List;
  std::string busyMessage;
  int busyProgressPercent = -1;  // -1 = no progress bar, just the message
  bool shouldTearDownWifiOnExit = false;
  std::vector<GutenbergBook> popularBooks;  // in-memory only, loaded fresh each "Sync Now"

  // Marquee-scrolls the selected row's title when it's too long to fit - shared
  // mechanism, see ScrollingListRow.h.
  MarqueeScroller scroller;
  std::string contentLabelFor(int index) const;

  static constexpr int kPopularCount = GutenbergPaths::kPopularCount;
  static int contentItemCount() { return 1 + kPopularCount; }  // Random Book, Popular Book x N
  static int actionItemCount() { return 2; }  // Sync Now (loads the list, not the books), Manage Books
  static int itemCount() { return contentItemCount() + actionItemCount(); }

  void ensureWifiThen(const std::function<void()>& action);
  void loadPopularList();
  void openRandomBook();
  void openPopularBook(int index);
  void promptLoadList();

 public:
  explicit GutenbergActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Gutenberg", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

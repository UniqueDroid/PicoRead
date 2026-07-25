#pragma once
#include <string>
#include <vector>

#include "../Activity.h"
#include "components/ScrollingListRow.h"
#include "util/ButtonNavigator.h"

// Reached by selecting a feed in RssFeedListActivity: lists every synced
// article's title (from the feed's index.txt, written alongside each article
// during sync - see RssFeedListActivity::syncOneFeed) so a feed with many
// articles doesn't dump you straight into the first one. Selecting an entry
// opens that article directly; TxtReaderActivity's own Back handling still
// returns to the top-level RSS overview rather than back here (see
// isRssArticle() in TxtReaderActivity.cpp) - a possible follow-up, not done yet.
class RssArticleListActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  const size_t feedIndex;
  std::string feedTitle;
  std::string dir;
  std::vector<std::string> titles;

  // Marquee-scrolls the selected row's title when it's too long to fit - shared
  // mechanism, see ScrollingListRow.h.
  MarqueeScroller scroller;

  void loadTitles();

 public:
  RssArticleListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, size_t feedIndex)
      : Activity("RssArticleList", renderer, mappedInput), feedIndex(feedIndex) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

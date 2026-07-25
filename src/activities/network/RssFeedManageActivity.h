#pragma once

#include <string>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// "Manage Feeds" screen reached from the RSS tile: lists subscribed feeds plus a
// trailing "Delete All Feeds" row. Selecting a feed deletes it immediately (no
// confirmation step, matching OpdsSettingsActivity's delete convention); selecting
// "Delete All Feeds" clears every feed and all synced articles.
class RssFeedManageActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;

  // Marquee-scrolls the selected feed row's title when it's too long to fit -
  // same mechanism as RssFeedListActivity/RssArticleListActivity.
  size_t scrollTitleOffset = 0;
  unsigned long nextScrollStepMs = 0;
  void resetScroll();
  void stepScroll(const std::string& title);

  int itemCount() const;
  void deleteFeed(size_t feedIndex);
  void deleteAllFeeds();

 public:
  explicit RssFeedManageActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RssFeedManage", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

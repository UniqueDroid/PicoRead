#pragma once
#include <functional>
#include <string>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Home-screen "Wikipedia" tile: on-demand fetch of the Wikimedia REST API's
// featured-content feed (article/picture of the day), on-this-day digest, and a
// random article. No subscription/list state like the RSS tile - each entry just
// re-fetches and opens fresh content when selected. Uses the system's configured
// UI language to pick the Wikipedia edition.
class WikipediaActivity final : public Activity {
  enum class State { List, Busy };

  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  State state = State::List;
  std::string busyMessage;
  bool shouldTearDownWifiOnExit = false;

  static int itemCount() { return 4; }

  void ensureWifiThen(const std::function<void()>& action);
  void fetchArticleOfDay();
  void fetchPictureOfDay();
  void fetchOnThisDay();
  void fetchRandomArticle();

 public:
  explicit WikipediaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Wikipedia", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

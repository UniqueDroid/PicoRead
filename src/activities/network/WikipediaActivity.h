#pragma once
#include <functional>
#include <string>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Home-screen "Wikipedia" tile: fetches the Wikimedia REST API's featured
// article of the day, on-this-day digest, and a random article via a single
// "Sync Now" action, saving all of them to fixed SD paths so they're readable
// offline afterward - selecting an entry just opens the cached file, it does
// not re-fetch. Uses the system's configured UI language to pick the
// Wikipedia edition.
class WikipediaActivity final : public Activity {
  enum class State { List, Busy };

  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  State state = State::List;
  std::string busyMessage;
  bool shouldTearDownWifiOnExit = false;

  static int itemCount() { return 4; }

  void ensureWifiThen(const std::function<void()>& action);
  bool downloadArticleOfDay();
  bool downloadOnThisDay();
  bool downloadRandomArticle();
  void syncAll();
  void openTextOrPromptSync(const std::string& path);
  void promptSync();

 public:
  explicit WikipediaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Wikipedia", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

#pragma once
#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Home-screen "Gutenberg" tile: browses a fixed handful of public-domain books via
// the Gutendex API (https://gutendex.com - a JSON index over Project Gutenberg) -
// a random pick plus the current top-5 most-downloaded books - and downloads each
// one's real EPUB (not a plain-text dump), so it reads with proper
// chapters/table-of-contents through the existing EpubReaderActivity, no new
// reading UI needed. Mirrors WikipediaActivity: a single "Sync Now" action
// downloads everything for offline reading, selecting an entry never fetches on
// its own.
class GutenbergActivity final : public Activity {
  enum class State { List, Busy };

  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  State state = State::List;
  std::string busyMessage;
  bool shouldTearDownWifiOnExit = false;
  std::vector<std::string> popularTitles;  // cached for list labels once synced this session

  static constexpr int kPopularCount = 5;
  static int contentItemCount() { return 1 + kPopularCount; }  // Random Book, Popular Book x N
  static int actionItemCount() { return 1; }                   // Sync Now
  static int itemCount() { return contentItemCount() + actionItemCount(); }

  void ensureWifiThen(const std::function<void()>& action);
  bool downloadRandomBook();
  bool downloadPopularBooks();
  void syncAll();
  void openEpubOrPromptSync(const std::string& path);
  void promptSync();
  void loadPopularTitles();

 public:
  explicit GutenbergActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Gutenberg", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

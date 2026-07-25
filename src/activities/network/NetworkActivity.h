#pragma once
#include <functional>

#include "../Activity.h"

// Common base for activities that make network requests: connects to WiFi
// first (prompting via WifiSelectionActivity if not already connected)
// before running an action, and tears the connection back down on exit if
// this activity was the one that brought WiFi up. Shared by GutenbergActivity,
// WikipediaActivity, and RssFeedListActivity, which used to each duplicate
// this verbatim.
class NetworkActivity : public Activity {
 protected:
  bool shouldTearDownWifiOnExit = false;
  void ensureWifiThen(const std::function<void()>& action);

 public:
  using Activity::Activity;
  void onExit() override;
};

#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

struct RssFeed {
  std::string url;
  std::string title;  // Fetched from the feed's <title> when added; falls back to the URL.
};

// Subscribed RSS feed list, persisted alongside the other .picoread/*.json stores.
// Synced article text lives separately on SD under /.picoread/rss/<feedIndex>/ (see
// RssFeedListActivity) - this store only tracks the feed URLs/titles, not content.
class RssFeedStore : public PersistableStore<RssFeedStore> {
 private:
  std::vector<RssFeed> feeds;

  static constexpr size_t MAX_FEEDS = 20;

  RssFeedStore() = default;

  friend class PersistableStore<RssFeedStore>;

 public:
  static const char* getFilePath() { return "/.picoread/rss_feeds.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool addFeed(const RssFeed& feed);
  bool removeFeed(size_t index);

  const std::vector<RssFeed>& getFeeds() const { return feeds; }
  size_t getCount() const { return feeds.size(); }
};

#define RSS_STORE RssFeedStore::getInstance()

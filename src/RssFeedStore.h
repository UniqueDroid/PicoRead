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
  void clearAll();

  // Counts how many feeds a JSON file (see sdcard/rss_feeds_import.json for the
  // expected format) would add, without adding them - for a confirmation prompt
  // before importFromFile() actually runs.
  size_t previewImportCount(const char* path) const;

  // Bulk-adds feeds from a JSON file. Returns the number of feeds imported.
  size_t importFromFile(const char* path);

  const std::vector<RssFeed>& getFeeds() const { return feeds; }
  size_t getCount() const { return feeds.size(); }

  // Where synced articles for a given feed live on the SD card, named after the
  // feed itself (sanitized, disambiguated against same-named earlier feeds) -
  // see RssFeedListActivity/RssFeedManageActivity.
  static std::string articleDirFor(size_t feedIndex);
};

#define RSS_STORE RssFeedStore::getInstance()

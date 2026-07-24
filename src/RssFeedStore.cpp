#include "RssFeedStore.h"

#include <Logging.h>

#include <algorithm>

void RssFeedStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["feeds"].to<JsonArray>();
  for (const auto& feed : feeds) {
    JsonObject obj = arr.add<JsonObject>();
    obj["url"] = feed.url;
    obj["title"] = feed.title;
  }
}

bool RssFeedStore::fromJson(JsonVariantConst doc) {
  feeds.clear();
  JsonArrayConst arr = doc["feeds"].as<JsonArrayConst>();
  feeds.reserve(std::min(arr.size(), MAX_FEEDS));

  for (JsonObjectConst obj : arr) {
    if (feeds.size() >= MAX_FEEDS) break;
    RssFeed feed;
    feed.url = obj["url"] | "";
    feed.title = obj["title"] | "";
    feeds.push_back(std::move(feed));
  }

  LOG_DBG("RSS", "Loaded %zu RSS feeds from file", feeds.size());
  return true;
}

bool RssFeedStore::addFeed(const RssFeed& feed) {
  if (feeds.size() >= MAX_FEEDS) {
    LOG_DBG("RSS", "Cannot add more feeds, limit of %zu reached", MAX_FEEDS);
    return false;
  }
  feeds.push_back(feed);
  LOG_DBG("RSS", "Added feed: %s", feed.url.c_str());
  return saveToFile();
}

bool RssFeedStore::removeFeed(size_t index) {
  if (index >= feeds.size()) return false;
  LOG_DBG("RSS", "Removed feed: %s", feeds[index].url.c_str());
  feeds.erase(feeds.begin() + static_cast<ptrdiff_t>(index));
  return saveToFile();
}

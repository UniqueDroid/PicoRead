#pragma once

#include <string>

#include "StreamingJsonParser.h"

// Wikimedia REST API's /feed/featured/{date} bundles the featured article (tfa),
// picture of the day (image), most-read articles, news, and on-this-day events all
// in one response - the mostread/news sections alone can run to tens of KB, and
// buffering the whole thing before parsing risks the same OOM that hit RssParser's
// first version (see RssParser.h). This streams the response straight into the
// parser and only retains the two small sections actually used, discarding
// everything else as it's parsed rather than after.
class WikipediaFeaturedParser {
 public:
  WikipediaFeaturedParser();
  WikipediaFeaturedParser(const WikipediaFeaturedParser&) = delete;
  WikipediaFeaturedParser& operator=(const WikipediaFeaturedParser&) = delete;

  void feed(const char* data, size_t len);

  const std::string& getArticleTitle() const { return articleTitle; }
  const std::string& getArticleExtract() const { return articleExtract; }
  // Prefer the thumbnail (small, fast) over the full-resolution original.
  const std::string& getImageUrl() const { return thumbnailUrl.empty() ? fullImageUrl : thumbnailUrl; }

 private:
  enum class Position : uint8_t {
    TOP_LEVEL,
    IN_TFA,
    IN_IMAGE,
    IN_IMAGE_THUMBNAIL,
    IN_IMAGE_SOURCE,
  };
  enum class LastKey : uint8_t { NONE, TFA, IMAGE, TITLE, EXTRACT, THUMBNAIL, IMAGE_SOURCE, SOURCE };

  static void sOnKey(void* ctx, const char* key, size_t len);
  static void sOnString(void* ctx, const char* value, size_t len);
  static void sOnObjectStart(void* ctx);
  static void sOnObjectEnd(void* ctx);
  static void sOnArrayStart(void* ctx);
  static void sOnArrayEnd(void* ctx);

  StreamingJsonParser parser;

  Position position = Position::TOP_LEVEL;
  LastKey lastKey = LastKey::NONE;
  int depth = 0;       // generic skip-tracking while at TOP_LEVEL
  int tfaDepth = 0;     // nesting within "tfa" once entered
  int imageDepth = 0;   // nesting within "image" once entered
  int innerDepth = 0;   // nesting within "image.thumbnail" or "image.image" once entered

  std::string articleTitle;
  std::string articleExtract;
  std::string thumbnailUrl;
  std::string fullImageUrl;
};

// /feed/onthisday/selected/{mm}/{dd} -> {"selected": [{"year": 1234, "text": "...",
// "pages": [...]}, ...]}. Streams events straight into one combined digest string
// instead of collecting the array first - same reasoning as above, and matches how
// RssParser writes articles out one at a time rather than accumulating them.
class WikipediaOnThisDayParser {
 public:
  WikipediaOnThisDayParser();
  WikipediaOnThisDayParser(const WikipediaOnThisDayParser&) = delete;
  WikipediaOnThisDayParser& operator=(const WikipediaOnThisDayParser&) = delete;

  void feed(const char* data, size_t len);

  const std::string& getDigest() const { return digest; }
  int getEventCount() const { return eventCount; }

 private:
  enum class Position : uint8_t { TOP_LEVEL, IN_SELECTED_ARRAY, IN_EVENT_OBJECT };
  enum class LastKey : uint8_t { NONE, SELECTED, YEAR, TEXT };

  static void sOnKey(void* ctx, const char* key, size_t len);
  static void sOnString(void* ctx, const char* value, size_t len);
  static void sOnNumber(void* ctx, const char* value, size_t len);
  static void sOnObjectStart(void* ctx);
  static void sOnObjectEnd(void* ctx);
  static void sOnArrayStart(void* ctx);
  static void sOnArrayEnd(void* ctx);

  void commitEvent();

  static constexpr int MAX_EVENTS = 40;

  StreamingJsonParser parser;

  Position position = Position::TOP_LEVEL;
  LastKey lastKey = LastKey::NONE;
  int depth = 0;
  int eventDepth = 0;

  int currentYear = 0;
  std::string currentText;

  std::string digest;
  int eventCount = 0;
};

// /page/random/summary -> {"title": "...", "extract": "...", ...}, all top-level.
class WikipediaSummaryParser {
 public:
  WikipediaSummaryParser();
  WikipediaSummaryParser(const WikipediaSummaryParser&) = delete;
  WikipediaSummaryParser& operator=(const WikipediaSummaryParser&) = delete;

  void feed(const char* data, size_t len);

  const std::string& getTitle() const { return title; }
  const std::string& getExtract() const { return extract; }

 private:
  enum class LastKey : uint8_t { NONE, TITLE, EXTRACT };

  static void sOnKey(void* ctx, const char* key, size_t len);
  static void sOnString(void* ctx, const char* value, size_t len);
  static void sOnObjectStart(void* ctx);
  static void sOnObjectEnd(void* ctx);
  static void sOnArrayStart(void* ctx);
  static void sOnArrayEnd(void* ctx);

  StreamingJsonParser parser;

  LastKey lastKey = LastKey::NONE;
  int depth = 0;

  std::string title;
  std::string extract;
};

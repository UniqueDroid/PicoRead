#pragma once
#include <functional>
#include <string>
#include <vector>

struct RssArticle {
  std::string title;
  std::string link;
  std::string description;  // plain text, already HTML-stripped (see stripHtml() in Dictionary lib)
};

struct RssFeedData {
  std::string title;
  std::vector<RssArticle> articles;  // only populated when no ArticleHandler is set, see below
};

/**
 * Streaming RSS 2.0 (<rss><channel><item>) / Atom (<feed><entry>) parser.
 *
 * Some blogs embed full post HTML in their feed (seen in the wild: a single
 * WordPress feed north of a megabyte), and HttpDownloader::fetchUrl's
 * std::string overload buffers the whole response before handing it over -
 * on a ~380KB-RAM ESP32-C3 a single feed like that can fail the reallocation
 * outright and abort() the firmware. Feed this parser via feed() from
 * HttpDownloader's DataCallback overload instead, so the raw XML is never
 * held in memory as one contiguous buffer.
 *
 * The same problem applies one level up: collecting every parsed article into
 * feedData.articles before writing anything out can itself exceed the
 * available heap for feeds with many full-length posts (confirmed via two
 * device crash reports - the per-field/per-article caps in the .cpp bounded
 * each dimension individually but not their product). Set an ArticleHandler
 * to be called once per completed <item>/<entry> instead - e.g. write it
 * straight to SD and let it go out of scope - so RAM never holds more than
 * one article's worth of text regardless of feed size.
 */
class RssParser {
 public:
  RssParser();
  ~RssParser();
  RssParser(const RssParser&) = delete;
  RssParser& operator=(const RssParser&) = delete;

  using ArticleHandler = std::function<void(const RssArticle&)>;
  void setArticleHandler(ArticleHandler handler) { articleHandler = std::move(handler); }

  // Feed one chunk of the response as it arrives. Returns false on an XML
  // parse error - stop calling feed() and treat the parse as failed.
  bool feed(const uint8_t* data, size_t len);

  // Call once after the last feed(), with no further data, to flush expat's
  // internal state. Returns false on an XML parse error.
  bool finish();

  RssFeedData feedData;

  // Public so the free-function expat callbacks (userData = this) can reach
  // them; not part of the intended external API otherwise.
  enum class State {
    Root,
    Channel,  // RSS 2.0 <channel> or Atom <feed>
    ChannelTitle,
    Item,  // RSS <item> or Atom <entry>
    ItemTitle,
    ItemLink,
    ItemDescription,  // RSS <description> or Atom <summary>/<content>
  };
  State state = State::Root;
  RssArticle current;
  size_t articleCount = 0;  // completed articles so far, whether handled or stored
  ArticleHandler articleHandler;

 private:
  void* parser = nullptr;  // XML_Parser (expat.h stays out of the header)
};

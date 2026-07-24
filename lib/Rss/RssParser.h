#pragma once
#include <string>
#include <vector>

struct RssArticle {
  std::string title;
  std::string link;
  std::string description;  // plain text, already HTML-stripped (see stripHtml() in Dictionary lib)
};

struct RssFeedData {
  std::string title;
  std::vector<RssArticle> articles;
};

namespace RssParser {
// Parses RSS 2.0 (<rss><channel><item>) or Atom (<feed><entry>) XML held
// entirely in memory - feed documents are small enough that the streaming/
// disk-index machinery ContentOpfParser needs for EPUBs isn't warranted here.
// Returns false on an XML parse error (feedData may be partially populated).
bool parse(const std::string& xml, RssFeedData& feedData);
}  // namespace RssParser

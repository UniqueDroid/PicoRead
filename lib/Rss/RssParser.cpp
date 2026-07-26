#include "RssParser.h"

#include <DictionaryIndex.h>  // stripHtml()
#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "expat.h"

namespace {

using State = RssParser::State;

// Bounds worst-case RAM use per feed regardless of how large the source
// document is - see the RssParser.h comment for why this matters on this
// hardware. Cheap insurance beyond streaming the download itself: a single
// mega-post would otherwise still balloon one RssArticle.
//
// Title/link get a small cap - neither is ever legitimately more than a few
// hundred bytes, so there's no reason to reserve a large contiguous block for
// them. Description is the one field that can genuinely hold a full teaser or
// (for some blogs) a whole post, so it keeps a bigger budget - but even that
// was lowered from an earlier 4096, see the crash-report note below.
constexpr size_t MAX_TITLE_LEN = 300;
constexpr size_t MAX_LINK_LEN = 300;
constexpr size_t MAX_DESCRIPTION_LEN = 2048;
constexpr size_t MAX_ARTICLES = 50;

void appendCapped(std::string& dest, const char* s, int len, size_t maxLen) {
  if (dest.size() >= maxLen || len <= 0) return;
  const size_t room = maxLen - dest.size();
  dest.append(s, std::min(static_cast<size_t>(len), room));
}

void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts) {
  auto* p = static_cast<RssParser*>(userData);
  // expat with XML_ParserCreate(nullptr) doesn't do namespace processing, so
  // elements arrive as plain local names ("item", "title", ...) regardless of
  // any xmlns/prefix in the document - no atom: / rss: prefix stripping needed.

  if (p->state == State::Root && (strcmp(name, "channel") == 0 || strcmp(name, "feed") == 0)) {
    p->state = State::Channel;
    return;
  }

  if (p->state == State::Channel && (strcmp(name, "item") == 0 || strcmp(name, "entry") == 0)) {
    if (p->articleCount >= MAX_ARTICLES) return;  // ignore further items, cap already reached
    p->state = State::Item;
    p->current = RssArticle{};
    // Reserve each field's capacity once instead of letting appendCapped's
    // repeated appends grow it via ~doubling reallocations as expat feeds
    // content in one chunk at a time - each reallocation needs a new
    // contiguous heap block, and doing that for title/link/description on
    // every one of up to MAX_ARTICLES items per feed is real fragmentation
    // pressure.
    //
    // First round of this fix reserved a uniform 4096 bytes for all three
    // fields, which turned out to just move the failure: a live crash report
    // symbolized straight to this reserve() call itself (operator new failing
    // inside std::string::reserve, called from here) - meaning a 4096-byte
    // contiguous block wasn't reliably available either, so batching the
    // request into one larger allocation wasn't enough on its own. Asking for
    // less (300 bytes for the fields that never legitimately need more, a
    // lower 2048 cap for description) is far more likely to actually succeed.
    p->current.title.reserve(MAX_TITLE_LEN);
    p->current.link.reserve(MAX_LINK_LEN);
    p->current.description.reserve(MAX_DESCRIPTION_LEN);
    return;
  }

  if (p->state == State::Channel && strcmp(name, "title") == 0) {
    p->state = State::ChannelTitle;
    return;
  }

  if (p->state == State::Item) {
    if (strcmp(name, "title") == 0) {
      p->state = State::ItemTitle;
    } else if (strcmp(name, "link") == 0) {
      // Atom: <link href="..."/> (self-closing, no character data). RSS: <link>url</link>.
      for (int i = 0; atts[i]; i += 2) {
        if (strcmp(atts[i], "href") == 0) {
          appendCapped(p->current.link, atts[i + 1], static_cast<int>(strlen(atts[i + 1])), MAX_LINK_LEN);
        }
      }
      p->state = State::ItemLink;
    } else if (strcmp(name, "description") == 0 || strcmp(name, "summary") == 0 || strcmp(name, "content") == 0) {
      // Only take the first of summary/content/description seen per item.
      if (p->current.description.empty()) p->state = State::ItemDescription;
    }
  }
}

void XMLCALL characterData(void* userData, const XML_Char* s, const int len) {
  auto* p = static_cast<RssParser*>(userData);
  switch (p->state) {
    case State::ChannelTitle:
      appendCapped(p->feedData.title, s, len, MAX_TITLE_LEN);
      break;
    case State::ItemTitle:
      appendCapped(p->current.title, s, len, MAX_TITLE_LEN);
      break;
    case State::ItemLink:
      // Only append for RSS's <link>url</link>; Atom already set current.link
      // from the href attribute and has no character data here.
      appendCapped(p->current.link, s, len, MAX_LINK_LEN);
      break;
    case State::ItemDescription:
      appendCapped(p->current.description, s, len, MAX_DESCRIPTION_LEN);
      break;
    default:
      break;
  }
}

void XMLCALL endElement(void* userData, const XML_Char* name) {
  auto* p = static_cast<RssParser*>(userData);

  if (p->state == State::ChannelTitle && strcmp(name, "title") == 0) {
    p->feedData.title = stripHtml(p->feedData.title);
    p->state = State::Channel;
    return;
  }
  if (p->state == State::ItemTitle && strcmp(name, "title") == 0) {
    p->state = State::Item;
    return;
  }
  if (p->state == State::ItemLink && strcmp(name, "link") == 0) {
    p->state = State::Item;
    return;
  }
  if (p->state == State::ItemDescription &&
      (strcmp(name, "description") == 0 || strcmp(name, "summary") == 0 || strcmp(name, "content") == 0)) {
    p->state = State::Item;
    return;
  }
  if (p->state == State::Item && (strcmp(name, "item") == 0 || strcmp(name, "entry") == 0)) {
    if (!p->current.title.empty() && p->articleCount < MAX_ARTICLES) {
      p->current.title = stripHtml(p->current.title);
      p->current.description = stripHtml(p->current.description);
      p->articleCount++;
      // With a handler set, the article is written out (e.g. to SD) and discarded
      // here instead of accumulating in feedData.articles - see RssParser.h.
      if (p->articleHandler) {
        p->articleHandler(p->current);
      } else {
        p->feedData.articles.push_back(std::move(p->current));
      }
    }
    p->state = State::Channel;
    return;
  }
  if (p->state == State::Channel && (strcmp(name, "channel") == 0 || strcmp(name, "feed") == 0)) {
    p->state = State::Root;
    return;
  }
}

}  // namespace

RssParser::RssParser() {
  parser = XML_ParserCreate(nullptr);
  if (!parser) {
    LOG_ERR("RSS", "Couldn't allocate XML parser");
    return;
  }
  XML_SetUserData(static_cast<XML_Parser>(parser), this);
  XML_SetElementHandler(static_cast<XML_Parser>(parser), startElement, endElement);
  XML_SetCharacterDataHandler(static_cast<XML_Parser>(parser), characterData);
}

RssParser::~RssParser() {
  if (parser) XML_ParserFree(static_cast<XML_Parser>(parser));
}

bool RssParser::feed(const uint8_t* data, size_t len) {
  if (!parser) return false;
  const XML_Status status =
      XML_Parse(static_cast<XML_Parser>(parser), reinterpret_cast<const char*>(data), static_cast<int>(len), 0);
  if (status == XML_STATUS_ERROR) {
    LOG_ERR("RSS", "Parse error at line %lu: %s", XML_GetCurrentLineNumber(static_cast<XML_Parser>(parser)),
            XML_ErrorString(XML_GetErrorCode(static_cast<XML_Parser>(parser))));
    return false;
  }
  return true;
}

bool RssParser::finish() {
  if (!parser) return false;
  const XML_Status status = XML_Parse(static_cast<XML_Parser>(parser), nullptr, 0, 1);
  if (status == XML_STATUS_ERROR) {
    LOG_ERR("RSS", "Parse error at line %lu: %s", XML_GetCurrentLineNumber(static_cast<XML_Parser>(parser)),
            XML_ErrorString(XML_GetErrorCode(static_cast<XML_Parser>(parser))));
    return false;
  }
  return true;
}

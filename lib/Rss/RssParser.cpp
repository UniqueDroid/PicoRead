#include "RssParser.h"

#include <DictionaryIndex.h>  // stripHtml()
#include <Logging.h>

#include <cstring>

#include "expat.h"

namespace {

enum class State {
  Root,
  Channel,  // RSS 2.0 <channel> or Atom <feed>
  ChannelTitle,
  Item,  // RSS <item> or Atom <entry>
  ItemTitle,
  ItemLink,
  ItemDescription,  // RSS <description> or Atom <summary>/<content>
};

struct ParseContext {
  State state = State::Root;
  RssFeedData* feed = nullptr;
  RssArticle current;
  bool inItem = false;
};

void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts) {
  auto* ctx = static_cast<ParseContext*>(userData);
  // expat with XML_ParserCreate(nullptr) doesn't do namespace processing, so
  // elements arrive as plain local names ("item", "title", ...) regardless of
  // any xmlns/prefix in the document - no atom: / rss: prefix stripping needed.

  if (ctx->state == State::Root && (strcmp(name, "channel") == 0 || strcmp(name, "feed") == 0)) {
    ctx->state = State::Channel;
    return;
  }

  if (ctx->state == State::Channel && (strcmp(name, "item") == 0 || strcmp(name, "entry") == 0)) {
    ctx->state = State::Item;
    ctx->inItem = true;
    ctx->current = RssArticle{};
    return;
  }

  if (ctx->state == State::Channel && strcmp(name, "title") == 0) {
    ctx->state = State::ChannelTitle;
    return;
  }

  if (ctx->state == State::Item) {
    if (strcmp(name, "title") == 0) {
      ctx->state = State::ItemTitle;
    } else if (strcmp(name, "link") == 0) {
      // Atom: <link href="..."/> (self-closing, no character data). RSS: <link>url</link>.
      for (int i = 0; atts[i]; i += 2) {
        if (strcmp(atts[i], "href") == 0) {
          ctx->current.link = atts[i + 1];
        }
      }
      ctx->state = State::ItemLink;
    } else if (strcmp(name, "description") == 0 || strcmp(name, "summary") == 0 || strcmp(name, "content") == 0) {
      // Only take the first of summary/content/description seen per item.
      if (ctx->current.description.empty()) ctx->state = State::ItemDescription;
    }
  }
}

void XMLCALL characterData(void* userData, const XML_Char* s, const int len) {
  auto* ctx = static_cast<ParseContext*>(userData);
  switch (ctx->state) {
    case State::ChannelTitle:
      if (ctx->feed) ctx->feed->title.append(s, len);
      break;
    case State::ItemTitle:
      ctx->current.title.append(s, len);
      break;
    case State::ItemLink:
      // Only append for RSS's <link>url</link>; Atom already set current.link
      // from the href attribute and has no character data here.
      ctx->current.link.append(s, len);
      break;
    case State::ItemDescription:
      ctx->current.description.append(s, len);
      break;
    default:
      break;
  }
}

void XMLCALL endElement(void* userData, const XML_Char* name) {
  auto* ctx = static_cast<ParseContext*>(userData);

  if (ctx->state == State::ChannelTitle && strcmp(name, "title") == 0) {
    ctx->state = State::Channel;
    return;
  }
  if (ctx->state == State::ItemTitle && strcmp(name, "title") == 0) {
    ctx->state = State::Item;
    return;
  }
  if (ctx->state == State::ItemLink && strcmp(name, "link") == 0) {
    ctx->state = State::Item;
    return;
  }
  if (ctx->state == State::ItemDescription &&
      (strcmp(name, "description") == 0 || strcmp(name, "summary") == 0 || strcmp(name, "content") == 0)) {
    ctx->state = State::Item;
    return;
  }
  if (ctx->state == State::Item && (strcmp(name, "item") == 0 || strcmp(name, "entry") == 0)) {
    if (ctx->feed && !ctx->current.title.empty()) {
      ctx->current.description = stripHtml(ctx->current.description);
      ctx->feed->articles.push_back(std::move(ctx->current));
    }
    ctx->inItem = false;
    ctx->state = State::Channel;
    return;
  }
  if (ctx->state == State::Channel && (strcmp(name, "channel") == 0 || strcmp(name, "feed") == 0)) {
    ctx->state = State::Root;
    return;
  }
}

}  // namespace

bool RssParser::parse(const std::string& xml, RssFeedData& feedData) {
  XML_Parser parser = XML_ParserCreate(nullptr);
  if (!parser) {
    LOG_ERR("RSS", "Couldn't allocate XML parser");
    return false;
  }

  ParseContext ctx;
  ctx.feed = &feedData;

  XML_SetUserData(parser, &ctx);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, characterData);

  const XML_Status status = XML_Parse(parser, xml.data(), static_cast<int>(xml.size()), /*isFinal=*/1);
  const bool ok = status != XML_STATUS_ERROR;
  if (!ok) {
    LOG_ERR("RSS", "Parse error at line %lu: %s", XML_GetCurrentLineNumber(parser),
            XML_ErrorString(XML_GetErrorCode(parser)));
  }
  XML_ParserFree(parser);
  return ok;
}

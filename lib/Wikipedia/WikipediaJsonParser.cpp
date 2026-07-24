#include "WikipediaJsonParser.h"

#include <cstdlib>
#include <cstring>

namespace {
// Article extracts run well past StreamingJsonParser's 512-byte default (which
// would silently drop the whole token, see StreamingJsonParser.h) - large enough
// for a multi-paragraph extract, heap-allocated so it doesn't threaten the task
// stack.
constexpr size_t kExtractTokenBufSize = 4096;

bool keyIs(const char* key, size_t len, const char* literal) {
  const size_t litLen = strlen(literal);
  return len == litLen && memcmp(key, literal, litLen) == 0;
}
}  // namespace

// ============================================================================
// WikipediaFeaturedParser
// ============================================================================

WikipediaFeaturedParser::WikipediaFeaturedParser()
    : parser(JsonCallbacks{this, sOnKey, sOnString, nullptr, nullptr, nullptr, sOnObjectStart, sOnObjectEnd,
                           sOnArrayStart, sOnArrayEnd},
            kExtractTokenBufSize) {}

void WikipediaFeaturedParser::feed(const char* data, size_t len) { parser.feed(data, len); }

void WikipediaFeaturedParser::sOnKey(void* ctx, const char* key, size_t len) {
  auto* self = static_cast<WikipediaFeaturedParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth == 1) {
        if (keyIs(key, len, "tfa"))
          self->lastKey = LastKey::TFA;
        else if (keyIs(key, len, "image"))
          self->lastKey = LastKey::IMAGE;
        else
          self->lastKey = LastKey::NONE;
      }
      break;
    case Position::IN_TFA:
      if (self->tfaDepth == 1) {
        if (keyIs(key, len, "title"))
          self->lastKey = LastKey::TITLE;
        else if (keyIs(key, len, "extract"))
          self->lastKey = LastKey::EXTRACT;
        else
          self->lastKey = LastKey::NONE;
      }
      break;
    case Position::IN_IMAGE:
      if (self->imageDepth == 1) {
        if (keyIs(key, len, "thumbnail"))
          self->lastKey = LastKey::THUMBNAIL;
        else if (keyIs(key, len, "image"))
          self->lastKey = LastKey::IMAGE_SOURCE;
        else
          self->lastKey = LastKey::NONE;
      }
      break;
    case Position::IN_IMAGE_THUMBNAIL:
    case Position::IN_IMAGE_SOURCE:
      if (self->innerDepth == 1) {
        self->lastKey = keyIs(key, len, "source") ? LastKey::SOURCE : LastKey::NONE;
      }
      break;
  }
}

void WikipediaFeaturedParser::sOnString(void* ctx, const char* value, size_t len) {
  auto* self = static_cast<WikipediaFeaturedParser*>(ctx);
  switch (self->position) {
    case Position::IN_TFA:
      if (self->tfaDepth == 1) {
        if (self->lastKey == LastKey::TITLE) self->articleTitle.assign(value, len);
        else if (self->lastKey == LastKey::EXTRACT) self->articleExtract.assign(value, len);
      }
      break;
    case Position::IN_IMAGE_THUMBNAIL:
      if (self->innerDepth == 1 && self->lastKey == LastKey::SOURCE) self->thumbnailUrl.assign(value, len);
      break;
    case Position::IN_IMAGE_SOURCE:
      if (self->innerDepth == 1 && self->lastKey == LastKey::SOURCE) self->fullImageUrl.assign(value, len);
      break;
    default:
      break;
  }
  self->lastKey = LastKey::NONE;
}

void WikipediaFeaturedParser::sOnObjectStart(void* ctx) {
  auto* self = static_cast<WikipediaFeaturedParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->lastKey == LastKey::TFA && self->depth == 1) {
        self->position = Position::IN_TFA;
        self->tfaDepth = 1;
        self->articleTitle.clear();
        self->articleExtract.clear();
      } else if (self->lastKey == LastKey::IMAGE && self->depth == 1) {
        self->position = Position::IN_IMAGE;
        self->imageDepth = 1;
        self->thumbnailUrl.clear();
        self->fullImageUrl.clear();
      } else {
        self->depth++;
      }
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_TFA:
      self->tfaDepth++;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_IMAGE:
      if (self->lastKey == LastKey::THUMBNAIL && self->imageDepth == 1) {
        self->position = Position::IN_IMAGE_THUMBNAIL;
        self->innerDepth = 1;
      } else if (self->lastKey == LastKey::IMAGE_SOURCE && self->imageDepth == 1) {
        self->position = Position::IN_IMAGE_SOURCE;
        self->innerDepth = 1;
      } else {
        self->imageDepth++;
      }
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_IMAGE_THUMBNAIL:
    case Position::IN_IMAGE_SOURCE:
      self->innerDepth++;
      self->lastKey = LastKey::NONE;
      break;
  }
}

void WikipediaFeaturedParser::sOnObjectEnd(void* ctx) {
  auto* self = static_cast<WikipediaFeaturedParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth > 0) self->depth--;
      break;
    case Position::IN_TFA:
      self->tfaDepth--;
      if (self->tfaDepth == 0) self->position = Position::TOP_LEVEL;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_IMAGE:
      self->imageDepth--;
      if (self->imageDepth == 0) self->position = Position::TOP_LEVEL;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_IMAGE_THUMBNAIL:
    case Position::IN_IMAGE_SOURCE:
      self->innerDepth--;
      if (self->innerDepth == 0) self->position = Position::IN_IMAGE;
      self->lastKey = LastKey::NONE;
      break;
  }
}

void WikipediaFeaturedParser::sOnArrayStart(void* ctx) {
  auto* self = static_cast<WikipediaFeaturedParser*>(ctx);
  // Arrays only ever occur inside branches we skip generically (mostread/news are
  // top-level arrays; tfa/image never contain arrays we care about) - treat exactly
  // like an uninteresting nested object for depth-tracking purposes.
  switch (self->position) {
    case Position::TOP_LEVEL:
      self->depth++;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_TFA:
      self->tfaDepth++;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_IMAGE:
      self->imageDepth++;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_IMAGE_THUMBNAIL:
    case Position::IN_IMAGE_SOURCE:
      self->innerDepth++;
      self->lastKey = LastKey::NONE;
      break;
  }
}

void WikipediaFeaturedParser::sOnArrayEnd(void* ctx) { sOnObjectEnd(ctx); }

// ============================================================================
// WikipediaOnThisDayParser
// ============================================================================

WikipediaOnThisDayParser::WikipediaOnThisDayParser()
    : parser(JsonCallbacks{this, sOnKey, sOnString, sOnNumber, nullptr, nullptr, sOnObjectStart, sOnObjectEnd,
                           sOnArrayStart, sOnArrayEnd},
            kExtractTokenBufSize) {}

void WikipediaOnThisDayParser::feed(const char* data, size_t len) { parser.feed(data, len); }

void WikipediaOnThisDayParser::commitEvent() {
  if (eventCount >= MAX_EVENTS || currentText.empty()) {
    currentText.clear();
    currentYear = 0;
    return;
  }
  digest += std::to_string(currentYear) + ": " + currentText + "\n\n";
  eventCount++;
  currentText.clear();
  currentYear = 0;
}

void WikipediaOnThisDayParser::sOnKey(void* ctx, const char* key, size_t len) {
  auto* self = static_cast<WikipediaOnThisDayParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth == 1) {
        self->lastKey = keyIs(key, len, "selected") ? LastKey::SELECTED : LastKey::NONE;
      }
      break;
    case Position::IN_EVENT_OBJECT:
      if (self->eventDepth == 1) {
        if (keyIs(key, len, "year"))
          self->lastKey = LastKey::YEAR;
        else if (keyIs(key, len, "text"))
          self->lastKey = LastKey::TEXT;
        else
          self->lastKey = LastKey::NONE;
      }
      break;
    default:
      break;
  }
}

void WikipediaOnThisDayParser::sOnString(void* ctx, const char* value, size_t len) {
  auto* self = static_cast<WikipediaOnThisDayParser*>(ctx);
  if (self->position == Position::IN_EVENT_OBJECT && self->eventDepth == 1 && self->lastKey == LastKey::TEXT) {
    self->currentText.assign(value, len);
  }
  self->lastKey = LastKey::NONE;
}

void WikipediaOnThisDayParser::sOnNumber(void* ctx, const char* value, size_t /*len*/) {
  auto* self = static_cast<WikipediaOnThisDayParser*>(ctx);
  if (self->position == Position::IN_EVENT_OBJECT && self->eventDepth == 1 && self->lastKey == LastKey::YEAR) {
    self->currentYear = static_cast<int>(strtol(value, nullptr, 10));
  }
  self->lastKey = LastKey::NONE;
}

void WikipediaOnThisDayParser::sOnObjectStart(void* ctx) {
  auto* self = static_cast<WikipediaOnThisDayParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      self->depth++;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_SELECTED_ARRAY:
      self->position = Position::IN_EVENT_OBJECT;
      self->eventDepth = 1;
      self->currentText.clear();
      self->currentYear = 0;
      break;
    case Position::IN_EVENT_OBJECT:
      self->eventDepth++;
      self->lastKey = LastKey::NONE;
      break;
  }
}

void WikipediaOnThisDayParser::sOnObjectEnd(void* ctx) {
  auto* self = static_cast<WikipediaOnThisDayParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth > 0) self->depth--;
      break;
    case Position::IN_EVENT_OBJECT:
      self->eventDepth--;
      if (self->eventDepth == 0) {
        self->commitEvent();
        self->position = Position::IN_SELECTED_ARRAY;
      }
      self->lastKey = LastKey::NONE;
      break;
    default:
      break;
  }
}

void WikipediaOnThisDayParser::sOnArrayStart(void* ctx) {
  auto* self = static_cast<WikipediaOnThisDayParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->lastKey == LastKey::SELECTED && self->depth == 1) {
        self->position = Position::IN_SELECTED_ARRAY;
      } else {
        self->depth++;
      }
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_EVENT_OBJECT:
      self->eventDepth++;
      self->lastKey = LastKey::NONE;
      break;
    default:
      break;
  }
}

void WikipediaOnThisDayParser::sOnArrayEnd(void* ctx) {
  auto* self = static_cast<WikipediaOnThisDayParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth > 0) self->depth--;
      break;
    case Position::IN_SELECTED_ARRAY:
      self->position = Position::TOP_LEVEL;
      break;
    case Position::IN_EVENT_OBJECT:
      self->eventDepth--;
      self->lastKey = LastKey::NONE;
      break;
  }
}

// ============================================================================
// WikipediaSummaryParser
// ============================================================================

WikipediaSummaryParser::WikipediaSummaryParser()
    : parser(JsonCallbacks{this, sOnKey, sOnString, nullptr, nullptr, nullptr, sOnObjectStart, sOnObjectEnd,
                           sOnArrayStart, sOnArrayEnd},
            kExtractTokenBufSize) {}

void WikipediaSummaryParser::feed(const char* data, size_t len) { parser.feed(data, len); }

void WikipediaSummaryParser::sOnKey(void* ctx, const char* key, size_t len) {
  auto* self = static_cast<WikipediaSummaryParser*>(ctx);
  if (self->depth == 1) {
    if (keyIs(key, len, "title"))
      self->lastKey = LastKey::TITLE;
    else if (keyIs(key, len, "extract"))
      self->lastKey = LastKey::EXTRACT;
    else
      self->lastKey = LastKey::NONE;
  }
}

void WikipediaSummaryParser::sOnString(void* ctx, const char* value, size_t len) {
  auto* self = static_cast<WikipediaSummaryParser*>(ctx);
  if (self->depth == 1) {
    if (self->lastKey == LastKey::TITLE) self->title.assign(value, len);
    else if (self->lastKey == LastKey::EXTRACT) self->extract.assign(value, len);
  }
  self->lastKey = LastKey::NONE;
}

void WikipediaSummaryParser::sOnObjectStart(void* ctx) {
  auto* self = static_cast<WikipediaSummaryParser*>(ctx);
  self->depth++;
  self->lastKey = LastKey::NONE;
}

void WikipediaSummaryParser::sOnObjectEnd(void* ctx) {
  auto* self = static_cast<WikipediaSummaryParser*>(ctx);
  if (self->depth > 0) self->depth--;
}

void WikipediaSummaryParser::sOnArrayStart(void* ctx) {
  auto* self = static_cast<WikipediaSummaryParser*>(ctx);
  self->depth++;
  self->lastKey = LastKey::NONE;
}

void WikipediaSummaryParser::sOnArrayEnd(void* ctx) { sOnObjectEnd(ctx); }

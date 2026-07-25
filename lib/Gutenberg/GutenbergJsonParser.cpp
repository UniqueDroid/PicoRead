#include "GutenbergJsonParser.h"

#include <cstring>

namespace {
// Book titles/URLs comfortably fit in StreamingJsonParser's 512-byte default -
// unlike Wikipedia extracts, nothing here is free-form prose.
bool keyIs(const char* key, size_t len, const char* literal) {
  const size_t litLen = strlen(literal);
  return len == litLen && memcmp(key, literal, litLen) == 0;
}
}  // namespace

GutendexBooksParser::GutendexBooksParser(size_t maxBooks)
    : parser(JsonCallbacks{this, sOnKey, sOnString, nullptr, nullptr, nullptr, sOnObjectStart, sOnObjectEnd,
                           sOnArrayStart, sOnArrayEnd}),
      maxBooks(maxBooks) {
  books.reserve(maxBooks);
}

void GutendexBooksParser::feed(const char* data, size_t len) { parser.feed(data, len); }

void GutendexBooksParser::sOnKey(void* ctx, const char* key, size_t len) {
  auto* self = static_cast<GutendexBooksParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth == 1) {
        self->lastKey = keyIs(key, len, "results") ? LastKey::RESULTS : LastKey::NONE;
      }
      break;
    case Position::IN_BOOK:
      if (self->bookDepth == 1) {
        if (keyIs(key, len, "title"))
          self->lastKey = LastKey::TITLE;
        else if (keyIs(key, len, "authors"))
          self->lastKey = LastKey::AUTHORS;
        else if (keyIs(key, len, "formats"))
          self->lastKey = LastKey::FORMATS;
        else
          self->lastKey = LastKey::NONE;
      }
      break;
    case Position::IN_AUTHOR:
      if (self->authorsDepth == 1) {
        self->lastKey = keyIs(key, len, "name") ? LastKey::AUTHOR_NAME : LastKey::NONE;
      }
      break;
    case Position::IN_FORMATS:
      if (self->formatsDepth == 1) {
        self->lastKey = keyIs(key, len, "application/epub+zip") ? LastKey::FORMAT_EPUB : LastKey::NONE;
      }
      break;
    default:
      break;
  }
}

void GutendexBooksParser::sOnString(void* ctx, const char* value, size_t len) {
  auto* self = static_cast<GutendexBooksParser*>(ctx);
  switch (self->position) {
    case Position::IN_BOOK:
      if (self->bookDepth == 1 && self->lastKey == LastKey::TITLE) self->current.title.assign(value, len);
      break;
    case Position::IN_AUTHOR:
      if (self->authorsDepth == 1 && self->lastKey == LastKey::AUTHOR_NAME && !self->authorCaptured) {
        self->current.author.assign(value, len);
        self->authorCaptured = true;
      }
      break;
    case Position::IN_FORMATS:
      if (self->formatsDepth == 1 && self->lastKey == LastKey::FORMAT_EPUB) {
        self->current.epubUrl.assign(value, len);
      }
      break;
    default:
      break;
  }
  self->lastKey = LastKey::NONE;
}

void GutendexBooksParser::sOnObjectStart(void* ctx) {
  auto* self = static_cast<GutendexBooksParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      self->depth++;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_RESULTS:
      self->position = Position::IN_BOOK;
      self->bookDepth = 1;
      self->current = GutenbergBook{};
      self->authorCaptured = false;
      break;
    case Position::IN_BOOK:
      if (self->lastKey == LastKey::FORMATS && self->bookDepth == 1) {
        self->position = Position::IN_FORMATS;
        self->formatsDepth = 1;
      } else {
        self->bookDepth++;
      }
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_AUTHORS:
      self->position = Position::IN_AUTHOR;
      self->authorsDepth = 1;
      break;
    case Position::IN_AUTHOR:
      self->authorsDepth++;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_FORMATS:
      self->formatsDepth++;
      self->lastKey = LastKey::NONE;
      break;
  }
}

void GutendexBooksParser::sOnObjectEnd(void* ctx) {
  auto* self = static_cast<GutendexBooksParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth > 0) self->depth--;
      break;
    case Position::IN_BOOK:
      self->bookDepth--;
      if (self->bookDepth == 0) {
        if (self->books.size() < self->maxBooks && !self->current.title.empty()) {
          self->books.push_back(self->current);
        }
        self->position = Position::IN_RESULTS;
      }
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_AUTHOR:
      self->authorsDepth--;
      if (self->authorsDepth == 0) self->position = Position::IN_AUTHORS;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_FORMATS:
      self->formatsDepth--;
      if (self->formatsDepth == 0) self->position = Position::IN_BOOK;
      self->lastKey = LastKey::NONE;
      break;
    default:
      break;
  }
}

void GutendexBooksParser::sOnArrayStart(void* ctx) {
  auto* self = static_cast<GutendexBooksParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->lastKey == LastKey::RESULTS && self->depth == 1) {
        self->position = Position::IN_RESULTS;
      } else {
        self->depth++;
      }
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_BOOK:
      if (self->lastKey == LastKey::AUTHORS && self->bookDepth == 1) {
        self->position = Position::IN_AUTHORS;
      } else {
        self->bookDepth++;  // subjects/bookshelves/languages/summaries/etc - skipped generically
      }
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_AUTHOR:
      self->authorsDepth++;
      self->lastKey = LastKey::NONE;
      break;
    default:
      break;
  }
}

void GutendexBooksParser::sOnArrayEnd(void* ctx) {
  auto* self = static_cast<GutendexBooksParser*>(ctx);
  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth > 0) self->depth--;
      break;
    case Position::IN_RESULTS:
      self->position = Position::TOP_LEVEL;
      break;
    case Position::IN_BOOK:
      self->bookDepth--;
      if (self->bookDepth == 0) {
        if (self->books.size() < self->maxBooks && !self->current.title.empty()) {
          self->books.push_back(self->current);
        }
        self->position = Position::IN_RESULTS;
      }
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_AUTHORS:
      self->position = Position::IN_BOOK;
      break;
    case Position::IN_AUTHOR:
      self->authorsDepth--;
      self->lastKey = LastKey::NONE;
      break;
    default:
      break;
  }
}

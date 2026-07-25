#pragma once

#include <string>
#include <vector>

#include "StreamingJsonParser.h"

struct GutenbergBook {
  std::string title;
  std::string author;  // first author only, e.g. "Austen, Jane"
  std::string epubUrl;
};

// Gutendex's /books/ endpoint (https://gutendex.com/books/) returns
// {"count":N,"next":"...","results":[{...book...}, ...]}. Each book has an
// "authors" array (only the first entry's "name" is kept) and a "formats" object
// keyed by mime type, from which only "application/epub+zip" is kept - the rest
// (subjects, bookshelves, summaries, other formats) is skipped via generic
// depth-tracking, same approach as WikipediaFeaturedParser. Streams straight from
// HttpDownloader rather than buffering: a full page of ~32 books with all their
// metadata can run past what's safe to hold in one std::string on this device.
class GutendexBooksParser {
 public:
  explicit GutendexBooksParser(size_t maxBooks);
  GutendexBooksParser(const GutendexBooksParser&) = delete;
  GutendexBooksParser& operator=(const GutendexBooksParser&) = delete;

  void feed(const char* data, size_t len);

  const std::vector<GutenbergBook>& getBooks() const { return books; }

 private:
  enum class Position : uint8_t { TOP_LEVEL, IN_RESULTS, IN_BOOK, IN_AUTHORS, IN_AUTHOR, IN_FORMATS };
  enum class LastKey : uint8_t { NONE, RESULTS, TITLE, AUTHORS, FORMATS, AUTHOR_NAME, FORMAT_EPUB };

  static void sOnKey(void* ctx, const char* key, size_t len);
  static void sOnString(void* ctx, const char* value, size_t len);
  static void sOnObjectStart(void* ctx);
  static void sOnObjectEnd(void* ctx);
  static void sOnArrayStart(void* ctx);
  static void sOnArrayEnd(void* ctx);

  StreamingJsonParser parser;

  Position position = Position::TOP_LEVEL;
  LastKey lastKey = LastKey::NONE;
  int depth = 0;         // generic skip-tracking while at TOP_LEVEL
  int bookDepth = 0;     // nesting within the current book object once entered
  int authorsDepth = 0;  // nesting within the current author object once entered
  int formatsDepth = 0;  // nesting within "formats" once entered

  const size_t maxBooks;
  GutenbergBook current;
  bool authorCaptured = false;
  std::vector<GutenbergBook> books;
};

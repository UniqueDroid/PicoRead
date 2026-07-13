#pragma once
#include <cstdint>
#include <string>

// Reads PicoRead's on-device dictionary format: a fixed-width binary
// index (index.didx) for O(log n) SD-card binary search, plus the
// definitions themselves (entries.dict), copied through unchanged from a
// source StarDict dictionary. See lib/Dictionary/README for the format and
// why it exists (in short: StarDict's own .idx has variable-length
// records, which rules out random-access binary search without loading
// the whole index into RAM). Produced by
// lib/Dictionary/scripts/convert_freedict.py.
struct DictionaryMeta {
  std::string bookName;
  uint32_t wordCount = 0;
  bool isHtml = false;  // entries.dict content is HTML, not plain text
};

class DictionaryIndex {
 public:
  // dirPath: SD folder containing dict.json, index.didx, entries.dict.
  bool open(const std::string& dirPath);
  void close();
  bool isOpen() const { return isOpen_; }

  const DictionaryMeta& getMeta() const { return meta; }

  // Case-insensitive (ASCII) exact-match lookup. On success, fills outText
  // with the raw definition (HTML if meta.isHtml - see stripHtml() below)
  // and returns true.
  bool lookup(const std::string& word, std::string& outText);

 private:
  std::string idxPath;
  std::string dictPath;
  DictionaryMeta meta;
  bool isOpen_ = false;
};

// Strips tags from a StarDict "h" (HTML) entry for plain-text display,
// collapsing <br>/<div>/<li> boundaries to newlines so list-like entries
// (FreeDict's cross-reference lists, multiple senses) stay readable.
std::string stripHtml(const std::string& html);

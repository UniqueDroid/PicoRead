#pragma once
#include <string>
#include <vector>

#include "DictionaryIndex.h"

// Enumerates installed dictionaries (used by both the web menu's
// Dictionaries page and the reader's auto-selection) and picks the best
// match for a book's language. Kept separate from DictionaryIndex, which
// only ever owns a single already-selected dictionary.
struct DictionaryListing {
  std::string id;  // folder name under /dictionaries/, e.g. "eng-deu"
  DictionaryMeta meta;
};

namespace DictionaryLibrary {

constexpr const char* DICTIONARIES_DIR = "/dictionaries";

// Scans /dictionaries/ for valid dictionary folders (each with a readable
// dict.json). Cheap: only reads the small dict.json per folder, never
// touches index.didx/entries.dict.
std::vector<DictionaryListing> listInstalled();

// Picks the best installed dictionary for a book written in `bookLanguage`
// (e.g. an EPUB's opf:language metadata - "en", "de", "en-US"). Matches the
// dictionary id's source-language segment ("eng-deu" -> "eng") via a small
// ISO 639-1 -> 639-3 table; falls back to the first installed dictionary if
// nothing matches or bookLanguage is unrecognized. Returns "" if none installed.
std::string pickBestDictionaryId(const std::vector<DictionaryListing>& installed, const std::string& bookLanguage);

}  // namespace DictionaryLibrary

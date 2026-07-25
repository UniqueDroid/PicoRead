#pragma once

#include <HalStorage.h>

#include <cstdio>
#include <string>
#include <vector>

#include "GutenbergJsonParser.h"

// Shared between GutenbergActivity (browse/download) and GutenbergManageActivity
// (delete/move downloaded books) so both agree on where things live without
// duplicating path literals.
namespace GutenbergPaths {
constexpr const char* kDir = "/.picoread/gutenberg";
constexpr const char* kRandomBookPath = "/.picoread/gutenberg/random.epub";
constexpr const char* kListPath = "/.picoread/gutenberg/list.txt";
constexpr const char* kLibraryPathFile = "/.picoread/gutenberg/library_path.txt";
constexpr const char* kDefaultLibraryPath = "/ebooks";
constexpr int kPopularCount = 5;

inline std::string popularBookPath(int index) {
  char buf[48];
  snprintf(buf, sizeof(buf), "/.picoread/gutenberg/popular%d.epub", index);
  return buf;
}

inline bool writeTextFile(const std::string& path, const std::string& content) {
  HalFile file;
  if (!Storage.openFileForWrite("GUTB", path, file)) return false;
  file.write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
  return true;
}

inline std::string readTextFile(const std::string& path) {
  HalFile file;
  if (!Storage.openFileForRead("GUTB", path, file)) return "";
  std::string content;
  char buf[256];
  size_t n;
  while ((n = file.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf))) > 0) {
    content.append(buf, n);
  }
  return content;
}

// Trims a single trailing newline, if any - readTextFile() returns the raw file
// content and library_path.txt is written with writeTextFile() (no helper adds
// one), but keeping this defensive costs nothing.
inline std::string loadLibraryPath() {
  std::string path = readTextFile(kLibraryPathFile);
  while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
  return path.empty() ? kDefaultLibraryPath : path;
}

inline bool saveLibraryPath(const std::string& path) { return writeTextFile(kLibraryPathFile, path); }

// Parses the "title\nurl\n" pairs written by GutenbergActivity::loadPopularList()
// - kept here so GutenbergManageActivity can look up titles for downloaded books
// without re-fetching or duplicating the parsing logic.
inline std::vector<GutenbergBook> loadPopularBooksFromDisk() {
  std::vector<GutenbergBook> books;
  const std::string raw = readTextFile(kListPath);
  size_t start = 0;
  while (books.size() < static_cast<size_t>(kPopularCount) && start < raw.size()) {
    const size_t titleEnd = raw.find('\n', start);
    if (titleEnd == std::string::npos) break;
    const size_t urlEnd = raw.find('\n', titleEnd + 1);
    if (urlEnd == std::string::npos) break;
    GutenbergBook book;
    book.title = raw.substr(start, titleEnd - start);
    book.epubUrl = raw.substr(titleEnd + 1, urlEnd - titleEnd - 1);
    if (!book.title.empty()) books.push_back(book);
    start = urlEnd + 1;
  }
  return books;
}
}  // namespace GutenbergPaths

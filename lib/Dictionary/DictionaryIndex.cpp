#include "DictionaryIndex.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace {
// Must match convert_freedict.py's WORD_SLOT_SIZE.
constexpr size_t WORD_SLOT_SIZE = 48;
constexpr size_t RECORD_SIZE = WORD_SLOT_SIZE + 4 + 4;  // word slot + uint32 offset + uint32 length (little-endian)

// ASCII-only case fold, matching how convert_freedict.py's source index was
// sorted (case-insensitive on the original StarDict data). UTF-8 continuation
// bytes and non-ASCII letters (e.g. German umlauts) are left as-is - accepted
// v1 limitation, see the library README.
char foldAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

int compareFolded(const char* recordWord, size_t recordWordLen, const std::string& needle) {
  const size_t n = std::min(recordWordLen, needle.size());
  for (size_t i = 0; i < n; i++) {
    const char a = foldAscii(recordWord[i]);
    const char b = foldAscii(needle[i]);
    if (a != b) return static_cast<unsigned char>(a) < static_cast<unsigned char>(b) ? -1 : 1;
  }
  if (recordWordLen == needle.size()) return 0;
  return recordWordLen < needle.size() ? -1 : 1;
}
}  // namespace

bool DictionaryIndex::open(const std::string& dirPath) {
  close();

  const std::string metaPath = dirPath + "/dict.json";
  String json = Storage.readFile(metaPath.c_str());
  if (json.isEmpty()) {
    LOG_ERR("DICT", "Failed to read %s", metaPath.c_str());
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, json.c_str())) {
    LOG_ERR("DICT", "Failed to parse %s", metaPath.c_str());
    return false;
  }

  meta.bookName = doc["bookName"] | dirPath;
  meta.wordCount = doc["wordCount"] | 0;
  meta.isHtml = doc["isHtml"] | false;

  if (meta.wordCount == 0) {
    LOG_ERR("DICT", "%s: wordCount is 0", metaPath.c_str());
    return false;
  }

  idxPath = dirPath + "/index.didx";
  dictPath = dirPath + "/entries.dict";
  if (!Storage.exists(idxPath.c_str()) || !Storage.exists(dictPath.c_str())) {
    LOG_ERR("DICT", "Missing index.didx or entries.dict under %s", dirPath.c_str());
    return false;
  }

  isOpen_ = true;
  LOG_DBG("DICT", "Opened dictionary: %s (%lu words)", meta.bookName.c_str(), static_cast<unsigned long>(meta.wordCount));
  return true;
}

void DictionaryIndex::close() {
  isOpen_ = false;
  idxPath.clear();
  dictPath.clear();
  meta = DictionaryMeta{};
}

std::string stripHtml(const std::string& html) {
  std::string out;
  out.reserve(html.size());

  size_t i = 0;
  while (i < html.size()) {
    if (html[i] != '<') {
      out += html[i];
      i++;
      continue;
    }

    const size_t close = html.find('>', i);
    const std::string tag = (close == std::string::npos) ? html.substr(i) : html.substr(i, close - i + 1);

    // Line-breaking tags: FreeDict entries pack multiple senses/cross-refs
    // into nested <div>/<li> - without a break they'd run together illegibly.
    if (tag.rfind("</div", 0) == 0 || tag.rfind("</li", 0) == 0 || tag.rfind("<br", 0) == 0) {
      if (!out.empty() && out.back() != '\n') out += '\n';
    } else if (!out.empty() && out.back() != ' ' && out.back() != '\n') {
      out += ' ';  // any other tag boundary still separates words
    }

    if (close == std::string::npos) break;
    i = close + 1;
  }

  // Decode the handful of entities StarDict/FreeDict entries actually use.
  static const std::pair<const char*, char> ENTITIES[] = {
      {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&#39;", '\''},
  };
  for (const auto& [entity, replacement] : ENTITIES) {
    size_t pos = 0;
    const size_t entityLen = strlen(entity);
    while ((pos = out.find(entity, pos)) != std::string::npos) {
      out.replace(pos, entityLen, 1, replacement);
      pos++;
    }
  }

  // Collapse runs of whitespace/newlines left by the tag-boundary spacing above.
  std::string collapsed;
  collapsed.reserve(out.size());
  bool lastWasSpace = false;
  bool lastWasNewline = false;
  for (char c : out) {
    if (c == '\n') {
      if (!lastWasNewline) collapsed += '\n';
      lastWasNewline = true;
      lastWasSpace = false;
    } else if (c == ' ' || c == '\t') {
      if (!lastWasSpace && !lastWasNewline) collapsed += ' ';
      lastWasSpace = true;
    } else {
      collapsed += c;
      lastWasSpace = false;
      lastWasNewline = false;
    }
  }
  while (!collapsed.empty() && (collapsed.front() == '\n' || collapsed.front() == ' ')) collapsed.erase(0, 1);
  while (!collapsed.empty() && (collapsed.back() == '\n' || collapsed.back() == ' ')) collapsed.pop_back();

  return collapsed;
}

bool DictionaryIndex::lookup(const std::string& word, std::string& outText) {
  if (!isOpen_ || word.empty()) {
    return false;
  }

  HalFile idxFile;
  if (!Storage.openFileForRead("DICT", idxPath, idxFile)) {
    LOG_ERR("DICT", "Failed to open %s", idxPath.c_str());
    return false;
  }

  uint8_t record[RECORD_SIZE];
  uint32_t lo = 0;
  uint32_t hi = meta.wordCount;  // exclusive upper bound
  uint32_t matchOffset = 0;
  uint32_t matchLength = 0;
  bool found = false;

  while (lo < hi) {
    const uint32_t mid = lo + (hi - lo) / 2;
    if (!idxFile.seek64(static_cast<uint64_t>(mid) * RECORD_SIZE)) {
      LOG_ERR("DICT", "Seek failed at record %lu", static_cast<unsigned long>(mid));
      return false;
    }
    if (idxFile.read(record, RECORD_SIZE) != static_cast<int>(RECORD_SIZE)) {
      LOG_ERR("DICT", "Short read at record %lu", static_cast<unsigned long>(mid));
      return false;
    }

    const char* recordWord = reinterpret_cast<const char*>(record);
    const void* nul = memchr(recordWord, '\0', WORD_SLOT_SIZE);
    const size_t recordWordLen = nul ? static_cast<const char*>(nul) - recordWord : WORD_SLOT_SIZE;

    const int cmp = compareFolded(recordWord, recordWordLen, word);
    if (cmp == 0) {
      uint32_t offset;
      uint32_t length;
      memcpy(&offset, record + WORD_SLOT_SIZE, 4);
      memcpy(&length, record + WORD_SLOT_SIZE + 4, 4);
      matchOffset = offset;
      matchLength = length;
      found = true;
      break;
    }
    if (cmp < 0) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  idxFile.close();

  if (!found) {
    return false;
  }
  if (matchLength == 0 || matchLength > 65536) {
    // Sanity bound: real dictionary entries are at most a few KB; a huge
    // length here means a corrupt/mismatched index.didx rather than a real
    // entry - refuse rather than attempting a multi-MB heap allocation.
    LOG_ERR("DICT", "Refusing implausible entry length %lu", static_cast<unsigned long>(matchLength));
    return false;
  }

  HalFile dictFile;
  if (!Storage.openFileForRead("DICT", dictPath, dictFile)) {
    LOG_ERR("DICT", "Failed to open %s", dictPath.c_str());
    return false;
  }
  if (!dictFile.seek64(matchOffset)) {
    LOG_ERR("DICT", "Seek failed in %s at %lu", dictPath.c_str(), static_cast<unsigned long>(matchOffset));
    dictFile.close();
    return false;
  }

  outText.resize(matchLength);
  const int bytesRead = dictFile.read(outText.data(), matchLength);
  dictFile.close();

  if (bytesRead != static_cast<int>(matchLength)) {
    LOG_ERR("DICT", "Short read for entry (%d/%lu bytes)", bytesRead, static_cast<unsigned long>(matchLength));
    outText.clear();
    return false;
  }

  return true;
}

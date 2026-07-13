#include "DictionaryLibrary.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>

namespace DictionaryLibrary {

std::vector<DictionaryListing> listInstalled() {
  std::vector<DictionaryListing> result;

  auto root = Storage.open(DICTIONARIES_DIR);
  if (!root || !root.isDirectory()) {
    return result;
  }
  root.rewindDirectory();

  char nameBuffer[160];  // function-local, bounded - plain stack buffer

  for (auto entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    if (!entry.isDirectory()) {
      continue;
    }
    entry.getName(nameBuffer, sizeof(nameBuffer));
    const std::string dirName(nameBuffer);
    const std::string metaPath = std::string(DICTIONARIES_DIR) + "/" + dirName + "/dict.json";

    String json = Storage.readFile(metaPath.c_str());
    if (json.isEmpty()) {
      continue;
    }
    JsonDocument doc;
    if (deserializeJson(doc, json.c_str())) {
      continue;
    }

    DictionaryListing listing;
    listing.id = dirName;
    listing.meta.bookName = doc["bookName"] | dirName;
    listing.meta.wordCount = doc["wordCount"] | 0;
    listing.meta.isHtml = doc["isHtml"] | false;
    result.push_back(std::move(listing));
  }
  root.close();

  return result;
}

namespace {
// ISO 639-1 (2-letter) -> ISO 639-3 (3-letter, matches FreeDict folder
// naming) for the languages PicoRead ships dictionaries for today.
// Extend this table as more language pairs are added.
struct LangCode {
  const char* iso639_1;
  const char* iso639_3;
};
constexpr LangCode LANG_CODES[] = {
    {"en", "eng"},
    {"de", "deu"},
};

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}
}  // namespace

std::string pickBestDictionaryId(const std::vector<DictionaryListing>& installed, const std::string& bookLanguage) {
  if (installed.empty()) {
    return "";
  }

  // Normalize "en-US" / "en_GB" etc. down to the primary subtag.
  std::string primary = toLower(bookLanguage);
  const size_t sep = primary.find_first_of("-_");
  if (sep != std::string::npos) {
    primary = primary.substr(0, sep);
  }

  std::string iso639_3;
  for (const auto& code : LANG_CODES) {
    if (primary == code.iso639_1) {
      iso639_3 = code.iso639_3;
      break;
    }
  }

  if (!iso639_3.empty()) {
    const std::string prefix = iso639_3 + "-";
    for (const auto& listing : installed) {
      if (listing.id.rfind(prefix, 0) == 0) {  // starts with prefix
        return listing.id;
      }
    }
  }

  LOG_DBG("DICT", "No dictionary matched book language '%s', falling back to first installed", bookLanguage.c_str());
  return installed.front().id;
}

}  // namespace DictionaryLibrary

#include "ReadingStatsStore.h"

#include <Logging.h>

#include <algorithm>

void ReadingStatsStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["books"].to<JsonArray>();
  for (const auto& b : books) {
    JsonObject obj = arr.add<JsonObject>();
    obj["path"] = b.bookPath;
    obj["sessions"] = b.sessions;
    obj["minutes"] = b.totalMinutes;
    obj["pages"] = b.totalPages;
  }
}

bool ReadingStatsStore::fromJson(JsonVariantConst doc) {
  books.clear();
  JsonArrayConst arr = doc["books"].as<JsonArrayConst>();
  books.reserve(std::min(arr.size(), MAX_TRACKED_BOOKS));
  for (JsonObjectConst obj : arr) {
    BookReadingStat stat;
    stat.bookPath = obj["path"] | "";
    stat.sessions = obj["sessions"] | 0;
    stat.totalMinutes = obj["minutes"] | 0;
    stat.totalPages = obj["pages"] | 0;
    if (!stat.bookPath.empty()) books.push_back(stat);
  }

  LOG_DBG("STATS", "Reading stats loaded from file (%d books)", static_cast<int>(books.size()));
  return true;
}

void ReadingStatsStore::addSession(const std::string& bookPath, uint16_t minutes, uint16_t pages) {
  if (minutes == 0 && pages == 0) return;

  auto it = std::find_if(books.begin(), books.end(), [&bookPath](const BookReadingStat& b) { return b.bookPath == bookPath; });
  if (it != books.end()) {
    it->sessions++;
    it->totalMinutes += minutes;
    it->totalPages += pages;
  } else {
    if (books.size() >= MAX_TRACKED_BOOKS) {
      // Drop the least-active tracked book to make room, rather than silently refusing
      // to track a book someone is actively reading right now.
      auto minIt = std::min_element(books.begin(), books.end(), [](const BookReadingStat& a, const BookReadingStat& b) {
        return a.totalMinutes < b.totalMinutes;
      });
      if (minIt != books.end()) books.erase(minIt);
    }
    BookReadingStat stat;
    stat.bookPath = bookPath;
    stat.sessions = 1;
    stat.totalMinutes = minutes;
    stat.totalPages = pages;
    books.push_back(stat);
  }

  saveToFile();
}

const BookReadingStat* ReadingStatsStore::getBook(const std::string& bookPath) const {
  auto it = std::find_if(books.begin(), books.end(), [&bookPath](const BookReadingStat& b) { return b.bookPath == bookPath; });
  return it != books.end() ? &(*it) : nullptr;
}

uint32_t ReadingStatsStore::totalSessions() const {
  uint32_t sum = 0;
  for (const auto& b : books) sum += b.sessions;
  return sum;
}

uint32_t ReadingStatsStore::totalMinutes() const {
  uint32_t sum = 0;
  for (const auto& b : books) sum += b.totalMinutes;
  return sum;
}

uint32_t ReadingStatsStore::totalPages() const {
  uint32_t sum = 0;
  for (const auto& b : books) sum += b.totalPages;
  return sum;
}

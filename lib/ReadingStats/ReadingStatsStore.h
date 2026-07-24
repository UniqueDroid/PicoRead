#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <string>
#include <vector>

struct BookReadingStat {
  std::string bookPath;  // key, matches Epub::getPath()
  uint32_t sessions = 0;
  uint32_t totalMinutes = 0;
  uint32_t totalPages = 0;
};

// Persists per-book reading session/time/page counts for the Reading Statistics tile.
// Keyed by book path, no calendar date involved - works the same on X3 and X4.
class ReadingStatsStore : public PersistableStore<ReadingStatsStore> {
 private:
  std::vector<BookReadingStat> books;

  // Bounds the file size; a personal library realistically never approaches this.
  static constexpr size_t MAX_TRACKED_BOOKS = 200;

  ReadingStatsStore() = default;
  ~ReadingStatsStore() = default;

  friend class PersistableStore<ReadingStatsStore>;

 public:
  static const char* getFilePath() { return "/.picoread/reading_stats.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // Adds one session's minutes/pages to bookPath's running totals (creating the entry
  // if needed) and increments its session count. No-op if minutes and pages are both 0.
  // Persists to file.
  void addSession(const std::string& bookPath, uint16_t minutes, uint16_t pages);

  // Clears all tracked reading stats for every book. Persists to file.
  void resetAll();

  // Returns nullptr if the book has no tracked reading.
  const BookReadingStat* getBook(const std::string& bookPath) const;

  const std::vector<BookReadingStat>& getBooks() const { return books; }

  uint32_t totalSessions() const;
  uint32_t totalMinutes() const;
  uint32_t totalPages() const;
};

#define READING_STATS ReadingStatsStore::getInstance()

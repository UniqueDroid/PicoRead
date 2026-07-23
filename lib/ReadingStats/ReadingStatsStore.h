#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <vector>

struct DailyReadingStat {
  uint32_t daysSinceEpoch = 0;  // days since 1970-01-01, per ReadingStatsStore::daysSinceEpoch()
  uint16_t minutes = 0;
  uint16_t pages = 0;
};

// Persists per-day reading time and page-turn counts, keyed by calendar date, for the
// Reading Statistics heatmap. Requires a calendar date (HalClock, X3-only) - callers on
// X4 (or an unsynced X3) should skip calling addSession() entirely.
class ReadingStatsStore : public PersistableStore<ReadingStatsStore> {
 private:
  std::vector<DailyReadingStat> days;  // sorted ascending by daysSinceEpoch

  // Bounds the file size and heatmap lookback window (~1 year + a bit of slack).
  static constexpr size_t MAX_TRACKED_DAYS = 371;

  ReadingStatsStore() = default;
  ~ReadingStatsStore() = default;

  friend class PersistableStore<ReadingStatsStore>;

 public:
  static const char* getFilePath() { return "/.picoread/reading_stats.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // Civil calendar date -> days since 1970-01-01 (Howard Hinnant's days_from_civil,
  // proleptic Gregorian, valid for any year - no dependency on time.h/mktime).
  static uint32_t daysSinceEpoch(uint16_t year, uint8_t month, uint8_t day);

  // Add reading time/pages to the given day, creating the entry if needed. Persists to file.
  // Entries older than MAX_TRACKED_DAYS from the newest tracked day are pruned on save.
  void addSession(uint32_t day, uint16_t minutes, uint16_t pages);

  // Returns nullptr if the day has no tracked reading.
  const DailyReadingStat* getDay(uint32_t day) const;

  const std::vector<DailyReadingStat>& getDays() const { return days; }
};

#define READING_STATS ReadingStatsStore::getInstance()

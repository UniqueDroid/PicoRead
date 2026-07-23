#include "ReadingStatsStore.h"

#include <Logging.h>

#include <algorithm>

uint32_t ReadingStatsStore::daysSinceEpoch(uint16_t year, uint8_t month, uint8_t day) {
  // Howard Hinnant's days_from_civil (proleptic Gregorian, valid for any date).
  const int y = year - (month <= 2 ? 1 : 0);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);                              // [0, 399]
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;           // [0, 365]
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                              // [0, 146096]
  return static_cast<uint32_t>(era * 146097 + static_cast<int>(doe) - 719468);             // since 1970-01-01
}

void ReadingStatsStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["days"].to<JsonArray>();
  for (const auto& d : days) {
    JsonObject obj = arr.add<JsonObject>();
    obj["d"] = d.daysSinceEpoch;
    obj["m"] = d.minutes;
    obj["p"] = d.pages;
  }
}

bool ReadingStatsStore::fromJson(JsonVariantConst doc) {
  days.clear();
  JsonArrayConst arr = doc["days"].as<JsonArrayConst>();
  days.reserve(std::min(arr.size(), MAX_TRACKED_DAYS));
  for (JsonObjectConst obj : arr) {
    DailyReadingStat stat;
    stat.daysSinceEpoch = obj["d"] | 0;
    stat.minutes = obj["m"] | 0;
    stat.pages = obj["p"] | 0;
    days.push_back(stat);
  }
  std::sort(days.begin(), days.end(),
            [](const DailyReadingStat& a, const DailyReadingStat& b) { return a.daysSinceEpoch < b.daysSinceEpoch; });

  LOG_DBG("STATS", "Reading stats loaded from file (%d days)", static_cast<int>(days.size()));
  return true;
}

void ReadingStatsStore::addSession(uint32_t day, uint16_t minutes, uint16_t pages) {
  if (minutes == 0 && pages == 0) return;

  auto it = std::find_if(days.begin(), days.end(), [day](const DailyReadingStat& d) { return d.daysSinceEpoch == day; });
  if (it != days.end()) {
    it->minutes += minutes;
    it->pages += pages;
  } else {
    DailyReadingStat stat;
    stat.daysSinceEpoch = day;
    stat.minutes = minutes;
    stat.pages = pages;
    days.push_back(stat);
    std::sort(days.begin(), days.end(), [](const DailyReadingStat& a, const DailyReadingStat& b) {
      return a.daysSinceEpoch < b.daysSinceEpoch;
    });
  }

  // Prune oldest entries beyond the tracked window.
  if (days.size() > MAX_TRACKED_DAYS) {
    days.erase(days.begin(), days.begin() + static_cast<long>(days.size() - MAX_TRACKED_DAYS));
  }

  saveToFile();
}

const DailyReadingStat* ReadingStatsStore::getDay(uint32_t day) const {
  auto it = std::find_if(days.begin(), days.end(), [day](const DailyReadingStat& d) { return d.daysSinceEpoch == day; });
  return it != days.end() ? &(*it) : nullptr;
}

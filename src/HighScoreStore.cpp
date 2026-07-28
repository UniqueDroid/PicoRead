#include "HighScoreStore.h"

#include <Logging.h>

#include <algorithm>

template <typename Tag>
void HighScoreStore<Tag>::toJson(JsonDocument& doc) const {
  doc["lastPlayerName"] = lastPlayerName;
  JsonArray arr = doc["scores"].to<JsonArray>();
  for (const auto& s : scores) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = s.name;
    obj["score"] = s.score;
  }
}

template <typename Tag>
bool HighScoreStore<Tag>::fromJson(JsonVariantConst doc) {
  lastPlayerName = doc["lastPlayerName"] | "";

  scores.clear();
  JsonArrayConst arr = doc["scores"].as<JsonArrayConst>();
  scores.reserve(std::min(arr.size(), MAX_SCORES));
  for (JsonObjectConst obj : arr) {
    if (scores.size() >= MAX_SCORES) break;
    Entry s;
    s.name = obj["name"] | "";
    s.score = obj["score"] | 0;
    scores.push_back(std::move(s));
  }

  LOG_DBG(HighScoreTraits<Tag>::logTag(), "Loaded %zu high scores", scores.size());
  return true;
}

template <typename Tag>
bool HighScoreStore<Tag>::qualifies(int score) const {
  if (score <= 0) return false;
  if (scores.size() < MAX_SCORES) return true;
  return score > scores.back().score;  // sorted descending, so back() is the lowest
}

template <typename Tag>
void HighScoreStore<Tag>::addScore(const std::string& name, int score) {
  lastPlayerName = name;
  scores.push_back(Entry{name, score});
  std::sort(scores.begin(), scores.end(),
            [](const Entry& a, const Entry& b) { return a.score > b.score; });
  if (scores.size() > MAX_SCORES) scores.resize(MAX_SCORES);
  this->saveToFile();
}

template class HighScoreStore<FlappyTag>;
template class HighScoreStore<TetrisTag>;

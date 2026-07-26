#include "TetrisHighScoreStore.h"

#include <Logging.h>

#include <algorithm>

void TetrisHighScoreStore::toJson(JsonDocument& doc) const {
  doc["lastPlayerName"] = lastPlayerName;
  JsonArray arr = doc["scores"].to<JsonArray>();
  for (const auto& s : scores) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = s.name;
    obj["score"] = s.score;
  }
}

bool TetrisHighScoreStore::fromJson(JsonVariantConst doc) {
  lastPlayerName = doc["lastPlayerName"] | "";

  scores.clear();
  JsonArrayConst arr = doc["scores"].as<JsonArrayConst>();
  scores.reserve(std::min(arr.size(), MAX_SCORES));
  for (JsonObjectConst obj : arr) {
    if (scores.size() >= MAX_SCORES) break;
    TetrisHighScore s;
    s.name = obj["name"] | "";
    s.score = obj["score"] | 0;
    scores.push_back(std::move(s));
  }

  LOG_DBG("TETRIS", "Loaded %zu high scores", scores.size());
  return true;
}

bool TetrisHighScoreStore::qualifies(int score) const {
  if (score <= 0) return false;
  if (scores.size() < MAX_SCORES) return true;
  return score > scores.back().score;  // sorted descending, so back() is the lowest
}

void TetrisHighScoreStore::addScore(const std::string& name, int score) {
  lastPlayerName = name;
  scores.push_back(TetrisHighScore{name, score});
  std::sort(scores.begin(), scores.end(),
           [](const TetrisHighScore& a, const TetrisHighScore& b) { return a.score > b.score; });
  if (scores.size() > MAX_SCORES) scores.resize(MAX_SCORES);
  saveToFile();
}

#include "FlappyHighScoreStore.h"

#include <Logging.h>

#include <algorithm>

void FlappyHighScoreStore::toJson(JsonDocument& doc) const {
  doc["lastPlayerName"] = lastPlayerName;
  JsonArray arr = doc["scores"].to<JsonArray>();
  for (const auto& s : scores) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = s.name;
    obj["score"] = s.score;
  }
}

bool FlappyHighScoreStore::fromJson(JsonVariantConst doc) {
  lastPlayerName = doc["lastPlayerName"] | "";

  scores.clear();
  JsonArrayConst arr = doc["scores"].as<JsonArrayConst>();
  scores.reserve(std::min(arr.size(), MAX_SCORES));
  for (JsonObjectConst obj : arr) {
    if (scores.size() >= MAX_SCORES) break;
    FlappyHighScore s;
    s.name = obj["name"] | "";
    s.score = obj["score"] | 0;
    scores.push_back(std::move(s));
  }

  LOG_DBG("FLAPPY", "Loaded %zu high scores", scores.size());
  return true;
}

bool FlappyHighScoreStore::qualifies(int score) const {
  if (score <= 0) return false;
  if (scores.size() < MAX_SCORES) return true;
  return score > scores.back().score;  // sorted descending, so back() is the lowest
}

void FlappyHighScoreStore::addScore(const std::string& name, int score) {
  lastPlayerName = name;
  scores.push_back(FlappyHighScore{name, score});
  std::sort(scores.begin(), scores.end(),
           [](const FlappyHighScore& a, const FlappyHighScore& b) { return a.score > b.score; });
  if (scores.size() > MAX_SCORES) scores.resize(MAX_SCORES);
  saveToFile();
}

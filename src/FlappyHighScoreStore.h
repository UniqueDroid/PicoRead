#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

struct FlappyHighScore {
  std::string name;
  int score = 0;
};

// Local top-5 Flappy high score board, persisted on the SD card. No network
// involved - a global/shared leaderboard would need a small backend of its own
// rather than a write-capable credential baked into publicly distributed firmware.
class FlappyHighScoreStore : public PersistableStore<FlappyHighScoreStore> {
 private:
  std::vector<FlappyHighScore> scores;  // sorted descending by score
  std::string lastPlayerName;           // remembered so the name entry can be prefilled

  static constexpr size_t MAX_SCORES = 5;

  FlappyHighScoreStore() = default;

  friend class PersistableStore<FlappyHighScoreStore>;

 public:
  static const char* getFilePath() { return "/.picoread/flappy_scores.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // True if score would make it onto the board (room left, or beats the current
  // lowest entry).
  bool qualifies(int score) const;

  // Inserts, re-sorts, trims to MAX_SCORES, and persists. Also remembers name for
  // next time's prefill.
  void addScore(const std::string& name, int score);

  const std::vector<FlappyHighScore>& getScores() const { return scores; }
  const std::string& getLastPlayerName() const { return lastPlayerName; }
};

#define FLAPPY_SCORES FlappyHighScoreStore::getInstance()

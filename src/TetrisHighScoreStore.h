#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

struct TetrisHighScore {
  std::string name;
  int score = 0;
};

// Local top-5 Tetris high score board, persisted on the SD card - same
// approach as FlappyHighScoreStore (see that class for why there's no
// network leaderboard).
class TetrisHighScoreStore : public PersistableStore<TetrisHighScoreStore> {
 private:
  std::vector<TetrisHighScore> scores;  // sorted descending by score
  std::string lastPlayerName;           // remembered so the name entry can be prefilled

  static constexpr size_t MAX_SCORES = 5;

  TetrisHighScoreStore() = default;

  friend class PersistableStore<TetrisHighScoreStore>;

 public:
  static const char* getFilePath() { return "/.picoread/tetris_scores.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // True if score would make it onto the board (room left, or beats the current
  // lowest entry).
  bool qualifies(int score) const;

  // Inserts, re-sorts, trims to MAX_SCORES, and persists. Also remembers name for
  // next time's prefill.
  void addScore(const std::string& name, int score);

  const std::vector<TetrisHighScore>& getScores() const { return scores; }
  const std::string& getLastPlayerName() const { return lastPlayerName; }
};

#define TETRIS_SCORES TetrisHighScoreStore::getInstance()

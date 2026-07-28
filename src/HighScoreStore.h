#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>
#include <vector>

// Local top-5 high score board, persisted on the SD card. No network involved -
// a global/shared leaderboard would need a small backend of its own rather than
// a write-capable credential baked into publicly distributed firmware.
//
// One template instantiation per minigame (see FlappyTag/TetrisTag below) - the
// only per-game difference is the save path and log tag, both supplied via
// HighScoreTraits<Tag>. Method bodies live in HighScoreStore.cpp with explicit
// instantiation there, so the template isn't re-emitted per translation unit.
template <typename Tag>
struct HighScoreEntry {
  std::string name;
  int score = 0;
};

template <typename Tag>
struct HighScoreTraits;  // specialized per game below

template <typename Tag>
class HighScoreStore : public PersistableStore<HighScoreStore<Tag>> {
 private:
  using Entry = HighScoreEntry<Tag>;
  std::vector<Entry> scores;   // sorted descending by score
  std::string lastPlayerName;  // remembered so the name entry can be prefilled

  static constexpr size_t MAX_SCORES = 5;

  HighScoreStore() = default;

  friend class PersistableStore<HighScoreStore<Tag>>;

 public:
  static const char* getFilePath() { return HighScoreTraits<Tag>::filePath(); }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // True if score would make it onto the board (room left, or beats the current
  // lowest entry).
  bool qualifies(int score) const;

  // Inserts, re-sorts, trims to MAX_SCORES, and persists. Also remembers name for
  // next time's prefill.
  void addScore(const std::string& name, int score);

  const std::vector<Entry>& getScores() const { return scores; }
  const std::string& getLastPlayerName() const { return lastPlayerName; }
};

struct FlappyTag {};
struct TetrisTag {};

template <>
struct HighScoreTraits<FlappyTag> {
  static const char* filePath() { return "/.picoread/flappy_scores.json"; }
  static const char* logTag() { return "FLAPPY"; }
};

template <>
struct HighScoreTraits<TetrisTag> {
  static const char* filePath() { return "/.picoread/tetris_scores.json"; }
  static const char* logTag() { return "TETRIS"; }
};

using FlappyHighScoreStore = HighScoreStore<FlappyTag>;
using TetrisHighScoreStore = HighScoreStore<TetrisTag>;

#define FLAPPY_SCORES FlappyHighScoreStore::getInstance()
#define TETRIS_SCORES TetrisHighScoreStore::getInstance()

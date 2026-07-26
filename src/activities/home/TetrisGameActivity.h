#pragma once
#include "../Activity.h"

// A small "5 minutes in between books" filler game (Left/Right = move,
// Down = soft drop, Up or Confirm = rotate). Like FlappyGameActivity, this
// runs on a fixed tick (TICK_MS) instead of a real animation loop - this
// display's fastest refresh mode still takes ~500ms end to end, nowhere near
// what a smooth 60Hz Tetris would need. Unlike Flappy though, Tetris is
// inherently grid-based/turn-based already, so a slow tick is a natural fit
// rather than a compromise.
class TetrisGameActivity final : public Activity {
  static constexpr int BOARD_COLS = 10;
  static constexpr int BOARD_ROWS = 20;
  static constexpr unsigned long NORMAL_TICK_MS = 800;
  static constexpr unsigned long SOFT_DROP_TICK_MS = 550;  // still above the ~500ms refresh floor

  // board[row][col]: 0 = empty, 1 = filled (locked piece cell). Row 0 is the top.
  uint8_t board[BOARD_ROWS][BOARD_COLS] = {};
  // Scratch buffer for clearCompletedLines() - a class member rather than a
  // ~200-byte local so the compaction pass never grows the call stack.
  uint8_t scratchBoard[BOARD_ROWS][BOARD_COLS] = {};

  int pieceType = 0;   // index into kPieces (0-6)
  int rotation = 0;    // 0-3
  int pieceX = 0;      // board column of the piece's 4x4 bounding box origin
  int pieceY = 0;      // board row of the piece's 4x4 bounding box origin
  int nextPieceType = 0;

  unsigned long lastTickMs = 0;
  bool gameOver = false;
  bool highScoreOffered = false;  // whether this game-over's name prompt has been shown yet
  int score = 0;
  int linesCleared = 0;

  int fieldX = 0;
  int contentTop = 0;
  int contentHeight = 0;
  int cellSize = 0;

  void resetGame();
  void spawnPiece();
  bool pieceFits(int type, int rot, int x, int y) const;
  void lockPiece();
  void clearCompletedLines();
  void tick();
  void startHighScoreEntry();

 public:
  explicit TetrisGameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TetrisGame", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

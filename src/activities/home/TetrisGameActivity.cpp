#include "TetrisGameActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MappedInputManager.h"
#include "HighScoreStore.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
struct Cell {
  int8_t dx, dy;
};
using Rotation = Cell[4];

constexpr int kPieceCount = 7;
// One 4x4-bounding-box offset list per rotation (0-3), per piece
// (I, O, T, S, Z, J, L). Plain in-place rotation, no SRS wall-kicks - a
// rotation that doesn't fit is simply rejected, which keeps the logic small
// and easy to verify by inspection.
constexpr Rotation kPieces[kPieceCount][4] = {
    // I
    {{{0, 1}, {1, 1}, {2, 1}, {3, 1}},
     {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
     {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
     {{2, 0}, {2, 1}, {2, 2}, {2, 3}}},
    // O
    {{{1, 0}, {2, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
    // T
    {{{0, 1}, {1, 1}, {2, 1}, {1, 0}},
     {{1, 0}, {1, 1}, {1, 2}, {2, 1}},
     {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
     {{1, 0}, {1, 1}, {1, 2}, {0, 1}}},
    // S
    {{{1, 0}, {2, 0}, {0, 1}, {1, 1}},
     {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
     {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
     {{1, 0}, {1, 1}, {2, 1}, {2, 2}}},
    // Z
    {{{0, 0}, {1, 0}, {1, 1}, {2, 1}},
     {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
     {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
     {{2, 0}, {1, 1}, {2, 1}, {1, 2}}},
    // J
    {{{0, 0}, {0, 1}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
     {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
     {{1, 0}, {1, 1}, {1, 2}, {0, 2}}},
    // L
    {{{2, 0}, {0, 1}, {1, 1}, {2, 1}},
     {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
     {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
     {{0, 0}, {1, 0}, {1, 1}, {1, 2}}},
};

// Classic-ish scoring: clearing several lines at once scores disproportionately
// more than clearing them one at a time. Index = number of lines cleared (0-4).
constexpr int kLineScore[5] = {0, 100, 300, 500, 800};
}  // namespace

bool TetrisGameActivity::pieceFits(const int type, const int rot, const int x, const int y) const {
  for (const auto& cell : kPieces[type][rot]) {
    const int col = x + cell.dx;
    const int row = y + cell.dy;
    if (col < 0 || col >= BOARD_COLS || row >= BOARD_ROWS) return false;
    if (row >= 0 && board[row][col]) return false;
  }
  return true;
}

void TetrisGameActivity::spawnPiece() {
  pieceType = nextPieceType;
  nextPieceType = std::rand() % kPieceCount;
  rotation = 0;
  pieceX = BOARD_COLS / 2 - 2;
  pieceY = -1;
}

void TetrisGameActivity::lockPiece() {
  for (const auto& cell : kPieces[pieceType][rotation]) {
    const int col = pieceX + cell.dx;
    const int row = pieceY + cell.dy;
    if (row >= 0 && row < BOARD_ROWS && col >= 0 && col < BOARD_COLS) {
      board[row][col] = 1;
    }
  }
}

void TetrisGameActivity::clearCompletedLines() {
  memset(scratchBoard, 0, sizeof(scratchBoard));
  int writeRow = BOARD_ROWS - 1;
  int cleared = 0;
  for (int row = BOARD_ROWS - 1; row >= 0; row--) {
    bool full = true;
    for (int col = 0; col < BOARD_COLS; col++) {
      if (!board[row][col]) {
        full = false;
        break;
      }
    }
    if (full) {
      cleared++;
      continue;
    }
    for (int col = 0; col < BOARD_COLS; col++) scratchBoard[writeRow][col] = board[row][col];
    writeRow--;
  }
  memcpy(board, scratchBoard, sizeof(board));

  if (cleared > 0) {
    linesCleared += cleared;
    score += kLineScore[std::min(cleared, 4)];
  }
}

void TetrisGameActivity::tick() {
  if (gameOver) return;

  if (pieceFits(pieceType, rotation, pieceX, pieceY + 1)) {
    pieceY++;
    return;
  }

  lockPiece();
  clearCompletedLines();
  spawnPiece();
  if (!pieceFits(pieceType, rotation, pieceX, pieceY)) {
    gameOver = true;
  }
}

void TetrisGameActivity::resetGame() {
  memset(board, 0, sizeof(board));
  gameOver = false;
  highScoreOffered = false;
  score = 0;
  linesCleared = 0;
  nextPieceType = std::rand() % kPieceCount;
  spawnPiece();
}

void TetrisGameActivity::onEnter() {
  Activity::onEnter();

  const auto& metrics = UITheme::getInstance().getMetrics();
  contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int scoreLineHeight = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  contentTop += scoreLineHeight;  // leave room for the score line above the board
  contentHeight = renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentWidth = renderer.getScreenWidth() - 2 * metrics.contentSidePadding;
  cellSize = std::max(4, std::min(contentWidth / BOARD_COLS, contentHeight / BOARD_ROWS));
  fieldX = metrics.contentSidePadding + (contentWidth - cellSize * BOARD_COLS) / 2;

  resetGame();
  lastTickMs = millis();
  requestUpdate();
}

void TetrisGameActivity::startHighScoreEntry() {
  const std::string prefill = TETRIS_SCORES.getLastPlayerName();
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TETRIS_HIGH_SCORE_NAME), prefill, 16,
                                              InputType::Text),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& kb = std::get<KeyboardResult>(result.data);
          if (!kb.text.empty()) {
            TETRIS_SCORES.addScore(kb.text, score);
          }
        }
        requestUpdate();
      });
}

void TetrisGameActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (gameOver) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      // First Confirm after Game Over just shows the popup (see render()); a
      // second Confirm either offers the name prompt (if the score qualifies)
      // or starts a new round directly - same two-step flow as FlappyGameActivity.
      if (!highScoreOffered) {
        highScoreOffered = true;
        if (TETRIS_SCORES.qualifies(score)) {
          startHighScoreEntry();
          return;
        }
      }
      resetGame();
      lastTickMs = millis();
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (pieceFits(pieceType, rotation, pieceX - 1, pieceY)) {
      pieceX--;
      requestUpdate();
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (pieceFits(pieceType, rotation, pieceX + 1, pieceY)) {
      pieceX++;
      requestUpdate();
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const int newRotation = (rotation + 1) % 4;
    if (pieceFits(pieceType, newRotation, pieceX, pieceY)) {
      rotation = newRotation;
      requestUpdate();
    }
  }

  const unsigned long tickInterval =
      mappedInput.isPressed(MappedInputManager::Button::Down) ? SOFT_DROP_TICK_MS : NORMAL_TICK_MS;
  const unsigned long now = millis();
  if (now - lastTickMs >= tickInterval) {
    lastTickMs = now;
    tick();
    requestUpdate();
  }
}

void TetrisGameActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TETRIS_GAME));

  char scoreBuf[32];
  snprintf(scoreBuf, sizeof(scoreBuf), tr(STR_SCORE_FORMAT), score);
  const int scoreY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  renderer.drawText(UI_10_FONT_ID, fieldX, scoreY, scoreBuf);

  const int fieldWidth = cellSize * BOARD_COLS;
  const int fieldHeight = cellSize * BOARD_ROWS;
  renderer.drawRect(fieldX, contentTop, fieldWidth, fieldHeight);

  for (int row = 0; row < BOARD_ROWS; row++) {
    for (int col = 0; col < BOARD_COLS; col++) {
      if (board[row][col]) {
        renderer.fillRect(fieldX + col * cellSize + 1, contentTop + row * cellSize + 1, cellSize - 2, cellSize - 2);
      }
    }
  }

  if (!gameOver) {
    for (const auto& cell : kPieces[pieceType][rotation]) {
      const int col = pieceX + cell.dx;
      const int row = pieceY + cell.dy;
      if (row >= 0 && row < BOARD_ROWS && col >= 0 && col < BOARD_COLS) {
        renderer.fillRect(fieldX + col * cellSize + 1, contentTop + row * cellSize + 1, cellSize - 2, cellSize - 2);
      }
    }
  } else {
    char popupBuf[64];
    snprintf(popupBuf, sizeof(popupBuf), "%s - %s", tr(STR_GAME_OVER), scoreBuf);
    GUI.drawPopup(renderer, popupBuf);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_ROTATE), tr(STR_LEFT), tr(STR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

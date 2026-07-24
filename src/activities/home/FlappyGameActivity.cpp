#include "FlappyGameActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>
#include <cstdlib>

#include "FlappyHighScoreStore.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "components/icons/flappy.h"
#include "fontIds.h"

void FlappyGameActivity::resetGame() {
  gameOver = false;
  started = false;
  flapLatched = false;
  score = 0;
  birdY = contentHeight / 2;
  spawnPipe();
}

void FlappyGameActivity::spawnPipe() {
  pipeX = contentWidth;
  // Keep the gap fully inside the playfield with a margin at each end.
  const int margin = 40;
  const int range = contentHeight - PIPE_GAP_HEIGHT - 2 * margin;
  pipeGapY = margin + (range > 0 ? std::rand() % range : 0);
}

void FlappyGameActivity::tick() {
  if (gameOver) return;

  if (flapLatched) {
    started = true;
    birdY -= FLAP_STEP;
    if (birdY < 0) birdY = 0;
    flapLatched = false;
  } else if (started) {
    birdY += GRAVITY_STEP;
  }

  if (birdY + BIRD_SIZE >= contentHeight) {
    birdY = contentHeight - BIRD_SIZE;
    if (started) gameOver = true;
  }

  if (!started) return;  // pipes stay put until the player takes off

  pipeX -= PIPE_SPEED_STEP;
  if (pipeX + PIPE_WIDTH < 0) {
    score++;
    spawnPipe();
    return;
  }

  const bool birdInPipeX = BIRD_X + BIRD_SIZE > pipeX && BIRD_X < pipeX + PIPE_WIDTH;
  if (birdInPipeX) {
    const bool inGap = birdY > pipeGapY && birdY + BIRD_SIZE < pipeGapY + PIPE_GAP_HEIGHT;
    if (!inGap) gameOver = true;
  }
}

void FlappyGameActivity::onEnter() {
  Activity::onEnter();

  const auto& metrics = UITheme::getInstance().getMetrics();
  contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  contentHeight = renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  contentWidth = renderer.getScreenWidth() - 2 * metrics.contentSidePadding;

  resetGame();
  lastTickMs = millis();
  requestUpdate();
}

void FlappyGameActivity::startHighScoreEntry() {
  const std::string prefill = FLAPPY_SCORES.getLastPlayerName();
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_FLAPPY_HIGH_SCORE_NAME), prefill, 16,
                                              InputType::Text),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& kb = std::get<KeyboardResult>(result.data);
          if (!kb.text.empty()) {
            FLAPPY_SCORES.addScore(kb.text, score);
          }
        }
        requestUpdate();
      });
}

void FlappyGameActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (gameOver) {
      resetGame();
      requestUpdate();
      return;
    }
    flapLatched = true;
  }

  const unsigned long now = millis();
  if (now - lastTickMs >= TICK_MS) {
    lastTickMs = now;
    const bool wasGameOver = gameOver;
    tick();
    requestUpdate();
    if (!wasGameOver && gameOver && FLAPPY_SCORES.qualifies(score)) {
      startHighScoreEntry();
    }
  }
}

void FlappyGameActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FLAPPY_GAME));

  const int fieldX = metrics.contentSidePadding;
  renderer.drawRect(fieldX, contentTop, contentWidth, contentHeight);

  // Pipe: two rectangles (top and bottom) with a PIPE_GAP_HEIGHT-tall gap between them.
  const int pipeScreenX = fieldX + pipeX;
  if (pipeScreenX + PIPE_WIDTH > fieldX && pipeScreenX < fieldX + contentWidth) {
    renderer.fillRect(pipeScreenX, contentTop, PIPE_WIDTH, pipeGapY);
    renderer.fillRect(pipeScreenX, contentTop + pipeGapY + PIPE_GAP_HEIGHT, PIPE_WIDTH,
                      contentHeight - pipeGapY - PIPE_GAP_HEIGHT);
  }

  // Before the first flap, the pipe hasn't started moving and the field is otherwise
  // empty - show the local high score board there instead of blank space.
  if (!started && !gameOver) {
    const auto& scores = FLAPPY_SCORES.getScores();
    if (!scores.empty()) {
      int listY = contentTop + 50;
      renderer.drawCenteredText(UI_10_FONT_ID, listY, tr(STR_FLAPPY_HIGH_SCORES), true, EpdFontFamily::BOLD);
      listY += 30;
      for (size_t i = 0; i < scores.size(); i++) {
        char rowBuf[48];
        snprintf(rowBuf, sizeof(rowBuf), "%d. %s - %d", static_cast<int>(i + 1), scores[i].name.c_str(), scores[i].score);
        renderer.drawCenteredText(UI_10_FONT_ID, listY, rowBuf);
        listY += 26;
      }
    }
  }

  renderer.drawIcon(FlappyIconGame, fieldX + BIRD_X, contentTop + birdY, BIRD_SIZE);

  char scoreBuf[32];
  snprintf(scoreBuf, sizeof(scoreBuf), tr(STR_SCORE_FORMAT), score);
  renderer.drawText(UI_10_FONT_ID, fieldX + 4, contentTop + 4, scoreBuf);

  if (gameOver) {
    renderer.drawText(UI_12_FONT_ID, fieldX + contentWidth / 2 - 40, contentTop + contentHeight / 2, tr(STR_GAME_OVER));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

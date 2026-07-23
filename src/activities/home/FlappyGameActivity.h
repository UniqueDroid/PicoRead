#pragma once
#include "../Activity.h"

// A small "5 minutes in between books" filler game (Confirm = flap). Unlike the
// original Flappy Bird, this runs on a slow fixed tick (TICK_MS) instead of a real
// animation loop: this display's fastest refresh mode still takes ~500ms end to end
// (see GfxRenderer/HalDisplay FAST_REFRESH usage elsewhere), nowhere near the ~30
// FPS a real-time port would need. A tick-based "step and redraw" game is what's
// actually achievable on this hardware.
class FlappyGameActivity final : public Activity {
  static constexpr unsigned long TICK_MS = 500;
  static constexpr int FLAP_STEP = 60;
  static constexpr int GRAVITY_STEP = 30;
  static constexpr int PIPE_SPEED_STEP = 50;
  static constexpr int PIPE_WIDTH = 60;
  static constexpr int PIPE_GAP_HEIGHT = 220;
  static constexpr int BIRD_SIZE = 40;  // matches FlappyIconGame's native 40x40 bitmap
  static constexpr int BIRD_X = 80;

  unsigned long lastTickMs = 0;
  bool flapLatched = false;
  bool gameOver = false;
  bool started = false;  // waits for the first flap before the pipe starts moving

  int birdY = 0;
  int pipeX = 0;
  int pipeGapY = 0;
  int score = 0;

  int contentTop = 0;
  int contentHeight = 0;
  int contentWidth = 0;

  void resetGame();
  void tick();
  void spawnPipe();

 public:
  explicit FlappyGameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FlappyGame", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

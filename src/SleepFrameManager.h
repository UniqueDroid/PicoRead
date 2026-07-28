#pragma once

class GfxRenderer;
class HalDisplay;

// Saves/restores the display framebuffer across deep sleep for "quick resume":
// the frame is written just before entering deep sleep and read back on the
// very next boot, so the screen can repaint instantly instead of showing the
// boot splash while the reader re-renders from scratch.
class SleepFrameManager {
 public:
  SleepFrameManager(GfxRenderer& renderer, HalDisplay& display) : _renderer(renderer), _display(display) {}

  // Writes the current framebuffer to SD. No-op (logged by HalStorage) if the
  // file can't be opened.
  void save();

  // Reads the saved framebuffer back into the display's buffer and removes the
  // file. Returns false if no file exists or its size doesn't match the
  // current buffer size (also removes the file in that case).
  bool restore();

 private:
  GfxRenderer& _renderer;
  HalDisplay& _display;
  static constexpr const char* PATH = "/.picoread/sleep_frame.bin";
};

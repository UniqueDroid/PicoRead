#pragma once
#include <string>

#include "../Activity.h"

// Home-screen tile showing reading session/time/page stats for the most recently
// read book plus a totals card across all tracked books (ReadingStatsStore).
class ReadingStatsActivity final : public Activity {
  void drawStatsCard(int x, int y, int width, const std::string& title, uint32_t sessions, uint32_t minutes,
                     uint32_t pages) const;
  void startResetFlow();

 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

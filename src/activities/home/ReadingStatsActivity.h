#pragma once
#include "../Activity.h"

// Home-screen tile (X3 only, requires the DS3231 RTC for a calendar date - see
// HalClock) showing a GitHub-contributions-style heatmap of daily reading time,
// plus today's totals and the current daily streak.
class ReadingStatsActivity final : public Activity {
  static constexpr int WEEKS_SHOWN = 24;  // ~5.5 months, sized to fit the content width

  void drawHeatmap(int x, int y, int width, int height) const;

 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

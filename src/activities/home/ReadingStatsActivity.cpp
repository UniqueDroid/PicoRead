#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "ReadingStatsStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Bucket thresholds (minutes read that day) -> shading, loosely modelled on GitHub's
// contribution heatmap. Four buckets fit comfortably in this display's dithered fills.
Color colorForMinutes(uint16_t minutes) {
  if (minutes == 0) return Color::White;
  if (minutes < 15) return Color::LightGray;
  if (minutes < 45) return Color::DarkGray;
  return Color::Black;
}
}  // namespace

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void ReadingStatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onGoHome();
  }
}

void ReadingStatsActivity::drawHeatmap(int x, int y, int width, int height) const {
  uint16_t year;
  uint8_t month, day;
  if (!halClock.isAvailable() || !halClock.getDate(year, month, day)) {
    renderer.drawText(UI_10_FONT_ID, x, y + 20, tr(STR_NO_UPDATE));  // RTC unsynced; nothing to show
    return;
  }

  const uint32_t today = ReadingStatsStore::daysSinceEpoch(year, month, day);
  constexpr int totalCells = WEEKS_SHOWN * 7;
  const uint32_t oldestDay = today >= (totalCells - 1) ? today - (totalCells - 1) : 0;

  const int cellSize = 12;
  const int gap = 3;
  const int pitch = cellSize + gap;
  // Center the grid horizontally within the given width.
  const int gridWidth = WEEKS_SHOWN * pitch - gap;
  const int startX = x + (width - gridWidth) / 2;

  for (int i = 0; i < totalCells; i++) {
    const uint32_t dayValue = oldestDay + i;
    const int col = i / 7;
    const int row = i % 7;
    const int cellX = startX + col * pitch;
    const int cellY = y + row * pitch;

    const DailyReadingStat* stat = READING_STATS.getDay(dayValue);
    const uint16_t minutes = stat ? stat->minutes : 0;
    renderer.fillRectDither(cellX, cellY, cellSize, cellSize, colorForMinutes(minutes));
    renderer.drawRect(cellX, cellY, cellSize, cellSize);  // outline, keeps empty cells visible as a grid
  }

  (void)height;
}

void ReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_READING_STATS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int heatmapHeight = 7 * (12 + 3) - 3;
  drawHeatmap(metrics.contentSidePadding, contentTop, pageWidth - 2 * metrics.contentSidePadding, heatmapHeight);

  uint16_t year;
  uint8_t month, day;
  if (halClock.isAvailable() && halClock.getDate(year, month, day)) {
    const uint32_t today = ReadingStatsStore::daysSinceEpoch(year, month, day);
    const DailyReadingStat* todayStat = READING_STATS.getDay(today);
    const int todayMinutes = todayStat ? todayStat->minutes : 0;
    const int todayPages = todayStat ? todayStat->pages : 0;

    int streak = 0;
    for (uint32_t d = today; READING_STATS.getDay(d) != nullptr; d--) {
      streak++;
      if (d == 0) break;  // avoid underflow at the epoch boundary
    }

    char buf[96];
    snprintf(buf, sizeof(buf), tr(STR_READING_STATS_SUMMARY_FORMAT), todayMinutes, todayPages, streak);
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + heatmapHeight + metrics.verticalSpacing * 3,
                      buf);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
  (void)pageHeight;
}

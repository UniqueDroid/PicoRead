#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "ReadingStatsStore.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
std::string formatDuration(uint32_t minutes) {
  char buf[24];
  if (minutes >= 60) {
    snprintf(buf, sizeof(buf), "%uh %02umin", static_cast<unsigned>(minutes / 60), static_cast<unsigned>(minutes % 60));
  } else {
    snprintf(buf, sizeof(buf), "%u min", static_cast<unsigned>(minutes));
  }
  return buf;
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

void ReadingStatsActivity::drawStatsCard(int x, int y, int width, const std::string& title, uint32_t sessions,
                                         uint32_t minutes, uint32_t pages) const {
  const int titleBarHeight = 34;
  const int rowHeight = 76;
  const int cardHeight = titleBarHeight + rowHeight * 2;

  renderer.drawRect(x, y, width, cardHeight);
  renderer.drawRect(x, y, width, titleBarHeight);

  const std::string truncated = renderer.truncatedText(UI_10_FONT_ID, title.c_str(), width - 16);
  renderer.drawText(UI_10_FONT_ID, x + 8, y + 9, truncated.c_str());

  const double avgSession = sessions > 0 ? static_cast<double>(minutes) / static_cast<double>(sessions) : 0.0;
  const double pagesPerMin = minutes > 0 ? static_cast<double>(pages) / static_cast<double>(minutes) : 0.0;

  char valueBuf[24];
  const int colWidth = width / 3;

  auto drawCell = [&](int col, int row, const char* value, StrId labelId) {
    const int cellX = x + col * colWidth;
    const int cellY = y + titleBarHeight + row * rowHeight;
    const int valueWidth = renderer.getTextAdvanceX(UI_12_FONT_ID, value, EpdFontFamily::REGULAR);
    const int labelWidth = renderer.getTextAdvanceX(UI_10_FONT_ID, I18N.get(labelId), EpdFontFamily::REGULAR);
    renderer.drawText(UI_12_FONT_ID, cellX + (colWidth - valueWidth) / 2, cellY + 14, value);
    renderer.drawText(UI_10_FONT_ID, cellX + (colWidth - labelWidth) / 2, cellY + 44, I18N.get(labelId));
  };

  snprintf(valueBuf, sizeof(valueBuf), "%u", static_cast<unsigned>(sessions));
  drawCell(0, 0, valueBuf, StrId::STR_SESSIONS);
  drawCell(1, 0, formatDuration(minutes).c_str(), StrId::STR_READING_TIME);
  snprintf(valueBuf, sizeof(valueBuf), "%u", static_cast<unsigned>(pages));
  drawCell(2, 0, valueBuf, StrId::STR_PAGES_TURNED);

  drawCell(0, 1, formatDuration(static_cast<uint32_t>(avgSession + 0.5)).c_str(), StrId::STR_AVG_SESSION);
  snprintf(valueBuf, sizeof(valueBuf), "%.1f", pagesPerMin);
  drawCell(1, 1, valueBuf, StrId::STR_PAGES_PER_MIN);
}

void ReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_READING_STATS));

  int cardY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int cardX = metrics.contentSidePadding;
  const int cardWidth = pageWidth - 2 * metrics.contentSidePadding;

  if (!RECENT_BOOKS.getBooks().empty()) {
    const RecentBook& current = RECENT_BOOKS.getBooks()[0];
    const BookReadingStat* stat = READING_STATS.getBook(current.path);
    drawStatsCard(cardX, cardY, cardWidth, current.title, stat ? stat->sessions : 0, stat ? stat->totalMinutes : 0,
                  stat ? stat->totalPages : 0);
    cardY += 34 + 76 * 2 + metrics.verticalSpacing * 2;
  }

  drawStatsCard(cardX, cardY, cardWidth, tr(STR_ALL_BOOKS), READING_STATS.totalSessions(), READING_STATS.totalMinutes(),
                READING_STATS.totalPages());

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

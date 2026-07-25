#include "ScrollingListRow.h"

#include <Arduino.h>

#include "fontIds.h"

namespace {
constexpr unsigned long kScrollStepMs = 480;
constexpr unsigned long kScrollPauseMs = 1200;
}  // namespace

namespace ScrollingListRow {

int rowHeight(const GfxRenderer& renderer) { return renderer.getLineHeight(UI_12_FONT_ID) + 16; }

void draw(const GfxRenderer& renderer, int pageWidth, int sidePadding, int rowY, int rowHeight,
         const std::string& text, bool selected) {
  if (selected) renderer.fillRect(0, rowY, pageWidth, rowHeight);
  const int maxWidth = pageWidth - sidePadding * 2;
  const std::string truncated = renderer.truncatedText(UI_12_FONT_ID, text.c_str(), maxWidth);
  const int textY = rowY + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, sidePadding, textY, truncated.c_str(), !selected);
}

}  // namespace ScrollingListRow

void MarqueeScroller::reset() {
  offset = 0;
  nextStepMs = millis() + kScrollPauseMs;
}

bool MarqueeScroller::step(const GfxRenderer& renderer, const std::string& text, int maxWidth) {
  if (renderer.getTextWidth(UI_12_FONT_ID, text.c_str()) <= maxWidth) return false;  // fits, nothing to scroll

  const unsigned long now = millis();
  if (now < nextStepMs) return false;

  size_t fitLen = 0;
  while (offset + fitLen < text.size()) {
    const std::string sub = text.substr(offset, fitLen + 1);
    if (renderer.getTextWidth(UI_12_FONT_ID, sub.c_str()) > maxWidth) break;
    fitLen++;
  }

  if (offset + fitLen >= text.size()) {
    // Reached the end of the text - pause there, then restart from the top.
    offset = 0;
    nextStepMs = now + kScrollPauseMs;
  } else {
    offset++;
    nextStepMs = now + kScrollStepMs;
  }
  return true;
}

std::string MarqueeScroller::visibleText(const std::string& fullText) const {
  return (offset > 0 && offset < fullText.size()) ? fullText.substr(offset) : fullText;
}

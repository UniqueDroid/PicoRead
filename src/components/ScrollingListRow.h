#pragma once

#include <GfxRenderer.h>

#include <string>

// Shared by every custom-rendered list screen that draws at the larger
// UI_12_FONT_ID instead of the shared GUI.drawList's fixed smaller font, with
// marquee-scrolling for a selected row whose text doesn't fit - originally
// written (and near-identically duplicated six times) across
// RssFeedListActivity, RssArticleListActivity, RssFeedManageActivity,
// GutenbergActivity, GutenbergManageActivity, and WikipediaActivity. This
// consolidates that into one place.
namespace ScrollingListRow {

// Row height for one line of UI_12_FONT_ID content plus vertical padding -
// use this consistently so every screen's pagination math agrees.
int rowHeight(const GfxRenderer& renderer);

// Draws one row: fills the selection background, truncates text to fit, and
// draws it inverted (white-on-black) when selected.
void draw(const GfxRenderer& renderer, int pageWidth, int sidePadding, int rowY, int rowHeight,
          const std::string& text, bool selected);

}  // namespace ScrollingListRow

// Per-selected-row marquee state: character-by-character scroll for text too
// long to fit at UI_12_FONT_ID, paced above this panel's ~450ms refresh time
// so update requests don't stack, with a pause at both ends of the scroll.
// One instance per list; call reset() whenever the selection changes and
// step() once per loop() tick (a no-op once the text already fits).
class MarqueeScroller {
 public:
  // Returns true if the caller should requestUpdate(true) - a scroll step
  // actually advanced this tick.
  bool step(const GfxRenderer& renderer, const std::string& text, int maxWidth);
  void reset();

  // The text to actually draw for the (possibly scrolled) selected row -
  // empty offset just returns the original string unchanged.
  std::string visibleText(const std::string& fullText) const;

 private:
  size_t offset = 0;
  unsigned long nextStepMs = 0;
};

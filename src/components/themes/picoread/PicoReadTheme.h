#pragma once

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// PicoRead theme metrics: BaseMetrics with a shorter "Continue Reading" cover
// area (frees up room for the 2-column tile grid below it) and 3 recent books
// shown side by side instead of 1.
namespace PicoReadMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = BaseMetrics::values;
  v.homeCoverHeight = 220;
  v.homeCoverTileHeight = 290;  // leaves ~70px below the cover image for the (up to 3-line) title
  v.homeRecentBooksCount = 3;
  return v;
}();
}  // namespace PicoReadMetrics

// PicoRead theme: identical to Classic (BaseTheme) everywhere except the home
// screen's button menu (2-column tile grid instead of a single vertical list)
// and the "Continue Reading" area (3 covers side by side instead of 1).
class PicoReadTheme : public BaseTheme {
 public:
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                           bool& bufferRestored, std::function<bool()> storeCoverBuffer) const override;
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const override;
};

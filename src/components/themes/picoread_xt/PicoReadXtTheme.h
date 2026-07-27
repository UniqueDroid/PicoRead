#pragma once

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// PicoRead-XT theme metrics: identical to PicoReadMetrics (see PicoReadTheme.h) -
// BaseMetrics with a shorter "Continue Reading" cover area and 3 recent books
// shown side by side instead of 1.
namespace PicoReadXtMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = BaseMetrics::values;
  v.homeCoverHeight = 220;
  v.homeCoverTileHeight = 290;  // leaves ~70px below the cover image for the (up to 3-line) title
  v.homeRecentBooksCount = 3;
  return v;
}();
}  // namespace PicoReadXtMetrics

// PicoRead-XT theme: identical copy of PicoReadTheme - same as Classic (BaseTheme)
// everywhere except the home screen's button menu (2-column tile grid instead of a
// single vertical list) and the "Continue Reading" area (3 covers side by side
// instead of 1).
class PicoReadXtTheme : public BaseTheme {
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

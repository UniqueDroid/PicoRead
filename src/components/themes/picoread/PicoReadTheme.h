#pragma once

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// PicoRead theme: identical to Classic (BaseTheme) everywhere except the home
// screen's button menu, which lays tiles out in a 2-column grid instead of a
// single vertical list. Reuses BaseMetrics rather than a new metrics struct
// since every other screen is intentionally unchanged from Classic.
class PicoReadTheme : public BaseTheme {
 public:
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
};

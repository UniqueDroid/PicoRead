#pragma once
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Settings > System > "Customize Home Menu": hide tiles that aren't used and
// reorder the rest. Settings itself is never listed here - see
// HomeMenuLayoutStore for why (it's the guaranteed way back in).
class HomeMenuCustomizeActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

 public:
  explicit HomeMenuCustomizeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("HomeMenuCustomize", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

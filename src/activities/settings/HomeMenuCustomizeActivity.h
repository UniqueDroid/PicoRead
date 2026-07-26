#pragma once
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Settings > System > "Customize Home Menu": hide tiles that aren't used and
// reorder all of them, including Settings itself - Settings can be moved but
// never hidden (HomeMenuLayoutStore::setVisible() silently refuses), so
// there's always a way back in to fix a bad layout.
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

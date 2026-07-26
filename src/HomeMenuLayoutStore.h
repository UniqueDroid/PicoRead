#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <vector>

#include "HomeMenuItem.h"
#include "components/themes/BaseTheme.h"  // UIIcon

// Display label/icon for a customizable HomeMenuItem - shared between
// HomeActivity (the actual tile grid) and HomeMenuCustomizeActivity (the
// hide/reorder settings screen) so the two never drift out of sync.
const char* homeMenuItemLabel(HomeMenuItem item);
UIIcon homeMenuItemIcon(HomeMenuItem item);

struct HomeMenuLayoutEntry {
  HomeMenuItem item = HomeMenuItem::NONE;
  bool visible = true;
};

// User-customizable home-screen tile order/visibility (Settings > System >
// "Customize Home Menu"). Only the reorderable/hideable tiles live here -
// Settings itself is always shown, always last, and never part of this list,
// so there's always a way back into Settings to re-enable something.
// Read by HomeActivity when building its menu; the "Continue Reading" cover
// strip and recent-book covers are a separate mechanism, not covered here.
class HomeMenuLayoutStore : public PersistableStore<HomeMenuLayoutStore> {
 private:
  std::vector<HomeMenuLayoutEntry> entries;

  HomeMenuLayoutStore();
  friend class PersistableStore<HomeMenuLayoutStore>;

 public:
  static const char* getFilePath() { return "/.picoread/home_menu.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  const std::vector<HomeMenuLayoutEntry>& getEntries() const { return entries; }

  void setVisible(size_t index, bool visible);
  // No-op at the top/bottom edge respectively.
  void moveUp(size_t index);
  void moveDown(size_t index);

  // The original fixed order, all visible - what a fresh install (and any
  // item unrecognized on load, e.g. from a downgrade) falls back to.
  static std::vector<HomeMenuItem> defaultOrder();
};

#define HOME_MENU_LAYOUT HomeMenuLayoutStore::getInstance()

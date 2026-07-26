#include "HomeMenuLayoutStore.h"

#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "fontIds.h"

const char* homeMenuItemLabel(HomeMenuItem item) {
  switch (item) {
    case HomeMenuItem::FILE_BROWSER:
      return tr(STR_BROWSE_FILES);
    case HomeMenuItem::RECENTS:
      return tr(STR_MENU_RECENT_BOOKS);
    case HomeMenuItem::ALL_BOOKMARKS:
      return tr(STR_BOOKMARKS);
    case HomeMenuItem::FLAPPY:
      return tr(STR_FLAPPY_GAME);
    case HomeMenuItem::TETRIS:
      return tr(STR_TETRIS_GAME);
    case HomeMenuItem::STATS:
      return tr(STR_READING_STATS);
    case HomeMenuItem::RSS_FEEDS:
      return tr(STR_RSS_FEEDS);
    case HomeMenuItem::WIKIPEDIA:
      return tr(STR_WIKIPEDIA);
    case HomeMenuItem::GUTENBERG:
      return tr(STR_GUTENBERG);
    case HomeMenuItem::OPDS_BROWSER:
      return tr(STR_OPDS_BROWSER);
    case HomeMenuItem::FILE_TRANSFER:
      return tr(STR_FILE_TRANSFER);
    case HomeMenuItem::SETTINGS_MENU:
      return tr(STR_SETTINGS_TITLE);
    default:
      return "";
  }
}

UIIcon homeMenuItemIcon(HomeMenuItem item) {
  switch (item) {
    case HomeMenuItem::FILE_BROWSER:
      return Folder;
    case HomeMenuItem::RECENTS:
      return Recent;
    case HomeMenuItem::ALL_BOOKMARKS:
      return Bookmark;
    case HomeMenuItem::FLAPPY:
      return Flappy;
    case HomeMenuItem::TETRIS:
      return Tetris;
    case HomeMenuItem::STATS:
      return Book;
    case HomeMenuItem::RSS_FEEDS:
      return Library;
    case HomeMenuItem::WIKIPEDIA:
      return Text;
    case HomeMenuItem::GUTENBERG:
      return Text;
    case HomeMenuItem::OPDS_BROWSER:
      return Library;
    case HomeMenuItem::FILE_TRANSFER:
      return Transfer;
    case HomeMenuItem::SETTINGS_MENU:
      return Settings;
    default:
      return None;
  }
}

namespace {
const char* itemToString(HomeMenuItem item) {
  switch (item) {
    case HomeMenuItem::FILE_BROWSER:
      return "FILE_BROWSER";
    case HomeMenuItem::RECENTS:
      return "RECENTS";
    case HomeMenuItem::ALL_BOOKMARKS:
      return "ALL_BOOKMARKS";
    case HomeMenuItem::FLAPPY:
      return "FLAPPY";
    case HomeMenuItem::TETRIS:
      return "TETRIS";
    case HomeMenuItem::STATS:
      return "STATS";
    case HomeMenuItem::RSS_FEEDS:
      return "RSS_FEEDS";
    case HomeMenuItem::WIKIPEDIA:
      return "WIKIPEDIA";
    case HomeMenuItem::GUTENBERG:
      return "GUTENBERG";
    case HomeMenuItem::OPDS_BROWSER:
      return "OPDS_BROWSER";
    case HomeMenuItem::FILE_TRANSFER:
      return "FILE_TRANSFER";
    case HomeMenuItem::SETTINGS_MENU:
      return "SETTINGS_MENU";
    default:
      return "";
  }
}

HomeMenuItem stringToItem(const char* s) {
  if (strcmp(s, "FILE_BROWSER") == 0) return HomeMenuItem::FILE_BROWSER;
  if (strcmp(s, "RECENTS") == 0) return HomeMenuItem::RECENTS;
  if (strcmp(s, "ALL_BOOKMARKS") == 0) return HomeMenuItem::ALL_BOOKMARKS;
  if (strcmp(s, "FLAPPY") == 0) return HomeMenuItem::FLAPPY;
  if (strcmp(s, "TETRIS") == 0) return HomeMenuItem::TETRIS;
  if (strcmp(s, "STATS") == 0) return HomeMenuItem::STATS;
  if (strcmp(s, "RSS_FEEDS") == 0) return HomeMenuItem::RSS_FEEDS;
  if (strcmp(s, "WIKIPEDIA") == 0) return HomeMenuItem::WIKIPEDIA;
  if (strcmp(s, "GUTENBERG") == 0) return HomeMenuItem::GUTENBERG;
  if (strcmp(s, "OPDS_BROWSER") == 0) return HomeMenuItem::OPDS_BROWSER;
  if (strcmp(s, "FILE_TRANSFER") == 0) return HomeMenuItem::FILE_TRANSFER;
  if (strcmp(s, "SETTINGS_MENU") == 0) return HomeMenuItem::SETTINGS_MENU;
  return HomeMenuItem::NONE;
}
}  // namespace

std::vector<HomeMenuItem> HomeMenuLayoutStore::defaultOrder() {
  return {HomeMenuItem::FILE_BROWSER, HomeMenuItem::RECENTS,       HomeMenuItem::ALL_BOOKMARKS,
          HomeMenuItem::FLAPPY,       HomeMenuItem::TETRIS,        HomeMenuItem::STATS,
          HomeMenuItem::RSS_FEEDS,    HomeMenuItem::WIKIPEDIA,     HomeMenuItem::GUTENBERG,
          HomeMenuItem::OPDS_BROWSER, HomeMenuItem::FILE_TRANSFER, HomeMenuItem::SETTINGS_MENU};
}

HomeMenuLayoutStore::HomeMenuLayoutStore() {
  const auto order = defaultOrder();
  entries.reserve(order.size());
  for (HomeMenuItem item : order) {
    entries.push_back({item, true});
  }
}

void HomeMenuLayoutStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["entries"].to<JsonArray>();
  for (const auto& e : entries) {
    JsonObject obj = arr.add<JsonObject>();
    obj["item"] = itemToString(e.item);
    obj["visible"] = e.visible;
  }
}

bool HomeMenuLayoutStore::fromJson(JsonVariantConst doc) {
  std::vector<HomeMenuLayoutEntry> loaded;
  JsonArrayConst arr = doc["entries"].as<JsonArrayConst>();
  loaded.reserve(arr.size());
  for (JsonObjectConst obj : arr) {
    const HomeMenuItem item = stringToItem(obj["item"] | "");
    if (item == HomeMenuItem::NONE) continue;  // unrecognized (e.g. a future downgrade) - drop it
    loaded.push_back({item, obj["visible"] | true});
  }

  // Any item missing from the saved file (a new tile added by a firmware
  // update since this was last saved) gets appended, visible by default, so
  // it actually shows up instead of silently disappearing.
  for (HomeMenuItem item : defaultOrder()) {
    const bool alreadyPresent =
        std::any_of(loaded.begin(), loaded.end(), [item](const HomeMenuLayoutEntry& e) { return e.item == item; });
    if (!alreadyPresent) loaded.push_back({item, true});
  }

  entries = std::move(loaded);
  // Defensive: Settings must never end up hidden, even from a hand-edited or
  // otherwise corrupted save file - it's the only guaranteed way back into
  // this screen to fix a bad layout.
  for (auto& e : entries) {
    if (e.item == HomeMenuItem::SETTINGS_MENU) e.visible = true;
  }
  LOG_DBG("HMENU", "Loaded %zu home menu entries", entries.size());
  return true;
}

void HomeMenuLayoutStore::setVisible(size_t index, bool visible) {
  if (index >= entries.size()) return;
  if (entries[index].item == HomeMenuItem::SETTINGS_MENU) return;  // never hideable
  entries[index].visible = visible;
  saveToFile();
}

void HomeMenuLayoutStore::moveUp(size_t index) {
  if (index == 0 || index >= entries.size()) return;
  std::swap(entries[index], entries[index - 1]);
  saveToFile();
}

void HomeMenuLayoutStore::moveDown(size_t index) {
  if (index + 1 >= entries.size()) return;
  std::swap(entries[index], entries[index + 1]);
  saveToFile();
}

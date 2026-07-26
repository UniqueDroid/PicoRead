#pragma once

// Split out from ActivityManager.h so lightweight consumers (e.g.
// HomeMenuLayoutStore) don't need to pull in the full Activity/ActivityManager
// class definitions just for this enum.
enum class HomeMenuItem {
  NONE,
  FILE_BROWSER,
  RECENTS,
  ALL_BOOKMARKS,
  FLAPPY,
  TETRIS,
  STATS,
  RSS_FEEDS,
  WIKIPEDIA,
  GUTENBERG,
  OPDS_BROWSER,
  FILE_TRANSFER,
  SETTINGS_MENU
};

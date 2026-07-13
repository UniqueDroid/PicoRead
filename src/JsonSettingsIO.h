#pragma once

#include <vector>

class PicoReadSettings;
class PicoReadState;
class WifiCredentialStore;
class RecentBooksStore;
class OpdsServerStore;
struct BookmarkEntry;

namespace JsonSettingsIO {

// PicoReadSettings
bool saveSettings(const PicoReadSettings& s, const char* path);
bool loadSettings(PicoReadSettings& s, const char* json, bool* needsResave = nullptr);

// PicoReadState
bool saveState(const PicoReadState& s, const char* path);
bool loadState(PicoReadState& s, const char* json);

// Bookmarks
bool saveBookmarks(const std::vector<BookmarkEntry>& bookmarks, const char* path);
bool loadBookmarks(std::vector<BookmarkEntry>& bookmarks, const char* json);

}  // namespace JsonSettingsIO

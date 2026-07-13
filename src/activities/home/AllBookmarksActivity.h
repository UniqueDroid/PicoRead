#pragma once
#include <string>
#include <vector>

#include "../../BookmarkEntry.h"
#include "../Activity.h"
#include "util/ButtonNavigator.h"

// A bookmark plus the book it belongs to, for the cross-book list this
// activity shows.
struct AggregatedBookmark {
  std::string bookPath;
  std::string bookTitle;
  BookmarkEntry entry;
};

// Home-screen tile that aggregates bookmarks set from within any book's
// reader menu (EpubReaderBookmarksActivity/"Toggle Bookmark") into one
// cross-book list. Selecting an entry resumes that book at the bookmarked
// position.
//
// Scope note: bookmarks are only surfaced here for books currently in the
// Recent Books list (RecentBooksStore, capped at 10) - the flattened
// on-disk bookmark filename (BookmarkUtil::getBookmarkPath) can't be
// reversed back into an exact source path, so Recent Books is the only
// available index of "book path -> possible bookmarks" without a full SD
// card scan. A book's bookmarks stop appearing here if it falls out of the
// last 10 (the bookmark file itself is untouched and still opens fine from
// the file browser).
class AllBookmarksActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  std::vector<AggregatedBookmark> bookmarks;

  void loadBookmarks();
  void onSelectBookmark(const AggregatedBookmark& bookmark);

 public:
  explicit AllBookmarksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("AllBookmarks", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

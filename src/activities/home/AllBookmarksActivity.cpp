#include "AllBookmarksActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <JsonSettingsIO.h>
#include <util/BookmarkUtil.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/reader/EpubReaderUtils.h"
#include "components/UITheme.h"
#include "fontIds.h"

void AllBookmarksActivity::loadBookmarks() {
  bookmarks.clear();

  // See the header comment: only books currently in Recent Books are
  // checked, since the flattened on-disk bookmark filename can't be
  // reversed back into an exact source path.
  for (const RecentBook& book : RECENT_BOOKS.getBooks()) {
    const std::string bookmarkPath = BookmarkUtil::getBookmarkPath(book.path);
    if (!Storage.exists(bookmarkPath.c_str())) {
      continue;
    }

    String json = Storage.readFile(bookmarkPath.c_str());
    if (json.isEmpty()) {
      continue;
    }

    std::vector<BookmarkEntry> bookBookmarks;
    JsonSettingsIO::loadBookmarks(bookBookmarks, json.c_str());
    for (auto& entry : bookBookmarks) {
      bookmarks.push_back({book.path, book.title, std::move(entry)});
    }
  }

  LOG_DBG("ABM", "Loaded %d bookmarks across %d recent books", static_cast<int>(bookmarks.size()),
          static_cast<int>(RECENT_BOOKS.getBooks().size()));
}

void AllBookmarksActivity::onSelectBookmark(const AggregatedBookmark& bookmark) {
  // Resume-on-open reads a book's own progress.bin (EpubReaderActivity::onEnter) -
  // overwrite it with this bookmark's position before opening, the same file the
  // book already resumes from normally. No new reader-side jump mechanism needed.
  if (bookmark.entry.computedChapterPageCount > 0) {
    Epub epub(bookmark.bookPath, "/.picoread");  // cheap: only computes the cache path, does not load the book
    EpubReaderUtils::saveProgress(epub, bookmark.entry.computedSpineIndex, bookmark.entry.computedChapterProgress,
                                  bookmark.entry.computedChapterPageCount);
  }
  activityManager.goToReader(bookmark.bookPath);
}

void AllBookmarksActivity::onEnter() {
  Activity::onEnter();
  loadBookmarks();
  selectorIndex = 0;
  requestUpdate();
}

void AllBookmarksActivity::onExit() {
  Activity::onExit();
  bookmarks.clear();
}

void AllBookmarksActivity::loop() {
  const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true);
  const int listSize = static_cast<int>(bookmarks.size());

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!bookmarks.empty() && selectorIndex < bookmarks.size()) {
      onSelectBookmark(bookmarks[selectorIndex]);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void AllBookmarksActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOKMARKS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (bookmarks.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_BOOKMARKS));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, bookmarks.size(), selectorIndex,
        [this](int index) { return bookmarks[index].bookTitle; },
        [this](int index) {
          const auto& bm = bookmarks[index].entry;
          const int pct = static_cast<int>(std::clamp(bm.percentage, 0.0f, 1.0f) * 100.0f + 0.5f);
          return std::to_string(pct) + "% - " + bm.summary;
        },
        [](int) { return UIIcon::Bookmark; });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

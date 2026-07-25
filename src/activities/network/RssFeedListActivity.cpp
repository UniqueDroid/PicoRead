#include "RssFeedListActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <RssParser.h>

#include <algorithm>
#include <cstdio>

#include "network/HttpDownloader.h"
#include "MappedInputManager.h"
#include "RssArticleListActivity.h"
#include "RssFeedManageActivity.h"
#include "RssFeedStore.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Dropped onto the SD card root by the user on their PC; see sdcard/rss_feeds_import.json for the format.
constexpr const char* kImportFilePath = "/rss_feeds_import.json";
}  // namespace

int RssFeedListActivity::feedRegionCount() const {
  const int feedCount = static_cast<int>(RSS_STORE.getCount());
  return feedCount == 0 ? 1 : feedCount;  // 1 = the "no feeds yet" placeholder row
}

int RssFeedListActivity::itemCount() const { return feedRegionCount() + actionRowCount(); }

void RssFeedListActivity::onEnter() {
  Activity::onEnter();
  state = State::List;
  selectorIndex = 0;
  scroller.reset();
  requestUpdate();
}

void RssFeedListActivity::startAddFeedFlow() {
  ensureWifiThen([this] {
    startActivityForResult(
        std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_RSS_FEED_URL), "https://", 200,
                                                InputType::Url),
        [this](const ActivityResult& result) {
          if (result.isCancelled) {
            requestUpdate();
            return;
          }
          const auto& kb = std::get<KeyboardResult>(result.data);
          onUrlEntered(kb.text);
        });
  });
}

void RssFeedListActivity::onUrlEntered(const std::string& url) {
  if (url.empty() || url == "https://" || url == "http://") {
    requestUpdate();
    return;
  }
  state = State::Busy;
  busyMessage = tr(STR_RSS_ADDING_FEED);
  requestUpdateAndWait();
  fetchAndAddFeed(url);
  state = State::List;
  requestUpdate();
}

void RssFeedListActivity::fetchAndAddFeed(const std::string& url) {
  // Streamed via the DataCallback overload rather than buffered into one
  // std::string: some feeds embed full post HTML and can run past a
  // megabyte, which fails outright on this device's heap (see RssParser.h).
  RssParser parser;
  const bool fetchOk = HttpDownloader::fetchUrl(
      url, [&parser](const uint8_t* data, size_t len) { return parser.feed(data, len); });
  if (!fetchOk) {
    LOG_ERR("RSS", "Failed to fetch feed: %s", url.c_str());
    return;
  }
  if (!parser.finish() || parser.feedData.title.empty()) {
    LOG_ERR("RSS", "Failed to parse feed or no title: %s", url.c_str());
  }

  RssFeed feed;
  feed.url = url;
  feed.title = parser.feedData.title.empty() ? url : parser.feedData.title;
  RSS_STORE.addFeed(feed);
}

void RssFeedListActivity::startSyncFlow() {
  if (RSS_STORE.getCount() == 0) return;
  ensureWifiThen([this] { syncAllFeeds(); });
}

void RssFeedListActivity::syncAllFeeds() {
  state = State::Busy;
  const auto& feeds = RSS_STORE.getFeeds();
  for (size_t i = 0; i < feeds.size(); i++) {
    char buf[96];
    snprintf(buf, sizeof(buf), tr(STR_RSS_SYNCING_FORMAT), static_cast<int>(i + 1), static_cast<int>(feeds.size()));
    busyMessage = buf;
    busyProgressPercent = static_cast<int>((i + 1) * 100 / feeds.size());
    requestUpdateAndWait();
    syncOneFeed(i);
  }
  busyProgressPercent = -1;
  state = State::List;
  requestUpdate();
}

bool RssFeedListActivity::syncOneFeed(size_t feedIndex) {
  const auto& feeds = RSS_STORE.getFeeds();
  if (feedIndex >= feeds.size()) return false;

  // Logged per-feed (not just once for the whole sync) so a crash report's
  // "Last logs" tail shows the heap trend leading up to whichever feed it died
  // on. Total free heap alone wasn't the story in the last field crash (83KB
  // free, plenty) - the panic PC symbolized to a std::string reallocation
  // inside expat's CDATA handling, which points at fragmentation rather than
  // exhaustion, so MaxAlloc (largest contiguous free block, same field
  // main.cpp already logs at boot) is logged too: a big gap between Free and
  // MaxAlloc would confirm it.
  LOG_DBG("RSS", "Heap before %s: free=%u maxAlloc=%u bytes", feeds[feedIndex].url.c_str(), ESP.getFreeHeap(),
         ESP.getMaxAllocHeap());

  const std::string dir = RssFeedStore::articleDirFor(feedIndex);
  // Clear out anything from a previous sync first: articles are written as
  // 0.txt, 1.txt, ... and a fresh sync just overwrites those indices, so a
  // feed that shrinks (fewer articles than last time) would otherwise leave
  // stale files past the new count sitting around forever, invisible to the
  // article list (which only goes by index.txt) but still taking up space.
  Storage.removeDir(dir.c_str());
  Storage.mkdir(dir.c_str(), true);

  // One title per line, written alongside each article rather than assembled in
  // RAM first (same streaming reasoning as the articles themselves) - lets
  // RssArticleListActivity show every article's title without opening all of
  // them just to read the first line.
  HalFile indexFile;
  Storage.openFileForWrite("RSS", dir + "/index.txt", indexFile);

  // Articles are written to SD as soon as each one finishes parsing, instead of
  // collecting them into feedData.articles first - some feeds run to dozens of
  // full-length posts, which doesn't fit in this device's heap if held all at
  // once (confirmed via crash reports even with per-field/article caps in
  // place). See RssParser.h.
  size_t articleIndex = 0;
  RssParser parser;
  parser.setArticleHandler([&dir, &articleIndex, &indexFile](const RssArticle& article) {
    const std::string path = dir + "/" + std::to_string(articleIndex) + ".txt";
    articleIndex++;
    HalFile file;
    if (!Storage.openFileForWrite("RSS", path, file)) return;
    file.write(reinterpret_cast<const uint8_t*>(article.title.data()), article.title.size());
    file.write(reinterpret_cast<const uint8_t*>("\n\n"), 2);
    if (!article.link.empty()) {
      file.write(reinterpret_cast<const uint8_t*>(article.link.data()), article.link.size());
      file.write(reinterpret_cast<const uint8_t*>("\n\n"), 2);
    }
    file.write(reinterpret_cast<const uint8_t*>(article.description.data()), article.description.size());

    // A title with an embedded newline (rare, but some feeds' CDATA titles span
    // lines) would otherwise desync the index's line count from the actual
    // number of article files - flatten to one line.
    std::string indexTitle = article.title.empty() ? "?" : article.title;
    std::replace(indexTitle.begin(), indexTitle.end(), '\n', ' ');
    indexFile.write(reinterpret_cast<const uint8_t*>(indexTitle.data()), indexTitle.size());
    indexFile.write(reinterpret_cast<const uint8_t*>("\n"), 1);
  });

  const bool fetchOk = HttpDownloader::fetchUrl(
      feeds[feedIndex].url, [&parser](const uint8_t* data, size_t len) { return parser.feed(data, len); });
  if (!fetchOk) {
    LOG_ERR("RSS", "Sync fetch failed: %s", feeds[feedIndex].url.c_str());
    return false;
  }
  if (!parser.finish()) {
    LOG_ERR("RSS", "Sync parse failed: %s", feeds[feedIndex].url.c_str());
    return false;
  }
  return true;
}

void RssFeedListActivity::startImportFlow() {
  // Preview first - Cancel bails out without touching anything, Confirm actually
  // imports. importFromFile() used to run unconditionally the moment this row was
  // tapped, with no way to back out.
  const size_t previewCount = RSS_STORE.previewImportCount(kImportFilePath);
  char previewBuf[64];
  snprintf(previewBuf, sizeof(previewBuf), tr(STR_RSS_IMPORT_PREVIEW_FORMAT), static_cast<int>(previewCount));
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_RSS_IMPORT_FROM_SD), previewBuf),
      [this](const ActivityResult& previewResult) {
        if (previewResult.isCancelled) {
          requestUpdate();
          return;
        }
        const size_t imported = RSS_STORE.importFromFile(kImportFilePath);
        char resultBuf[64];
        snprintf(resultBuf, sizeof(resultBuf), tr(STR_RSS_IMPORT_RESULT_FORMAT), static_cast<int>(imported));
        startActivityForResult(
            std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_RSS_IMPORT_FROM_SD), resultBuf),
            [this](const ActivityResult&) { requestUpdate(); });
      });
}

void RssFeedListActivity::startManageFeedsFlow() {
  startActivityForResult(std::make_unique<RssFeedManageActivity>(renderer, mappedInput),
                         [this](const ActivityResult&) {
                           // Feed count may have shrunk (deletions) - clamp so the
                           // selector doesn't point past the end of the new layout.
                           const int count = itemCount();
                           if (static_cast<int>(selectorIndex) >= count) {
                             selectorIndex = count > 0 ? static_cast<size_t>(count - 1) : 0;
                           }
                           requestUpdate();
                         });
}

void RssFeedListActivity::onSelectFeed(size_t feedIndex) {
  // Shows every synced article's title (RssArticleListActivity) instead of
  // jumping straight into the first one - some feeds run to dozens of articles
  // per sync, and picking a specific one beats paging through from the start.
  const std::string firstArticle = RssFeedStore::articleDirFor(feedIndex) + "/0.txt";
  if (!Storage.exists(firstArticle.c_str())) {
    // Not synced yet (e.g. just imported from SD): offer to sync right away
    // instead of just bouncing back to the list.
    startActivityForResult(
        std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_RSS_FEEDS), tr(STR_RSS_NOT_SYNCED_YET),
                                               tr(STR_CANCEL), tr(STR_RSS_SYNC_NOW_SHORT)),
        [this](const ActivityResult& result) {
          if (!result.isCancelled) {
            startSyncFlow();
          } else {
            requestUpdate();
          }
        });
    return;
  }
  startActivityForResult(std::make_unique<RssArticleListActivity>(renderer, mappedInput, feedIndex),
                         [this](const ActivityResult&) { requestUpdate(); });
}

void RssFeedListActivity::loop() {
  if (state != State::List) return;

  const int count = itemCount();
  const int feedRegion = feedRegionCount();
  const bool hasFeeds = RSS_STORE.getCount() > 0;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (static_cast<int>(selectorIndex) < feedRegion) {
      if (hasFeeds) onSelectFeed(selectorIndex);
      // else: "no feeds yet" placeholder, not actionable
    } else {
      switch (static_cast<int>(selectorIndex) - feedRegion) {
        case 0:
          startAddFeedFlow();
          break;
        case 1:
          startSyncFlow();
          break;
        case 2:
          startImportFlow();
          break;
        case 3:
          startManageFeedsFlow();
          break;
        default:
          break;
      }
    }
    return;
  }

  buttonNavigator.onNextRelease([this, count] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), count);
    scroller.reset();
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, count] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), count);
    scroller.reset();
    requestUpdate();
  });

  if (hasFeeds && static_cast<int>(selectorIndex) < feedRegion) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int maxWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
    if (scroller.step(renderer, RSS_STORE.getFeeds()[selectorIndex].title, maxWidth)) requestUpdate(true);
  }
}

void RssFeedListActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_RSS_FEEDS));

  if (state == State::Busy) {
    const Rect popupRect = GUI.drawPopup(renderer, busyMessage.c_str());
    if (busyProgressPercent >= 0) GUI.fillPopupProgress(renderer, popupRect, busyProgressPercent);
    return;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int feedRegion = feedRegionCount();
  const bool hasFeeds = RSS_STORE.getCount() > 0;
  const auto& feeds = RSS_STORE.getFeeds();

  // Custom rendering (not GUI.drawList, which hardcodes a smaller shared font)
  // so this screen can use the bigger UI_12_FONT_ID and marquee-scroll long
  // feed titles - see RssArticleListActivity for the same pattern.
  const int bigRowHeight = ScrollingListRow::rowHeight(renderer);
  const int actionsHeight = actionRowCount() * bigRowHeight;
  const int separatorGap = metrics.verticalSpacing;
  const int feedRegionHeight = std::max(bigRowHeight, contentBottom - contentTop - separatorGap - actionsHeight);

  const bool feedRegionFocused = static_cast<int>(selectorIndex) < feedRegion;

  const int feedPageItems = std::max(1, feedRegionHeight / bigRowHeight);
  const int feedPageStart =
      (feedRegionFocused ? static_cast<int>(selectorIndex) / feedPageItems * feedPageItems : 0);
  for (int i = feedPageStart; i < feedRegion && i < feedPageStart + feedPageItems; i++) {
    const int rowY = contentTop + (i - feedPageStart) * bigRowHeight;
    const bool selected = feedRegionFocused && i == static_cast<int>(selectorIndex);
    const std::string fullTitle = hasFeeds ? feeds[i].title : tr(STR_RSS_NO_FEEDS);
    const std::string text = selected ? scroller.visibleText(fullTitle) : fullTitle;
    ScrollingListRow::draw(renderer, pageWidth, metrics.contentSidePadding, rowY, bigRowHeight, text, selected);
  }

  const int separatorY = contentTop + feedRegionHeight + separatorGap / 2;
  renderer.drawLine(0, separatorY, pageWidth, separatorY);

  const int actionsTop = contentTop + feedRegionHeight + separatorGap;
  for (int i = 0; i < actionRowCount(); i++) {
    std::string label;
    switch (i) {
      case 0:
        label = I18N.get(StrId::STR_RSS_ADD_FEED);
        break;
      case 1:
        label = I18N.get(StrId::STR_RSS_SYNC_NOW);
        break;
      case 2:
        label = I18N.get(StrId::STR_RSS_IMPORT_FROM_SD);
        break;
      default:
        label = I18N.get(StrId::STR_RSS_MANAGE_FEEDS);
        break;
    }
    const int rowY = actionsTop + i * bigRowHeight;
    const bool selected = !feedRegionFocused && i == static_cast<int>(selectorIndex) - feedRegion;
    ScrollingListRow::draw(renderer, pageWidth, metrics.contentSidePadding, rowY, bigRowHeight, label, selected);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

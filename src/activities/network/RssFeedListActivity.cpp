#include "RssFeedListActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <RssParser.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>

#include "network/HttpDownloader.h"
#include "MappedInputManager.h"
#include "RssFeedManageActivity.h"
#include "RssFeedStore.h"
#include "activities/network/WifiSelectionActivity.h"
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
  requestUpdate();
}

void RssFeedListActivity::onExit() {
  Activity::onExit();
  if (shouldTearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
  }
}

void RssFeedListActivity::ensureWifiThen(const std::function<void()>& action) {
  if (WiFi.status() == WL_CONNECTED) {
    action();
    return;
  }
  shouldTearDownWifiOnExit = true;
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this, action](const ActivityResult& result) {
                           if (!result.isCancelled) {
                             action();
                           } else {
                             requestUpdate();
                           }
                         });
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

  const std::string dir = RssFeedStore::articleDirFor(feedIndex);
  Storage.mkdir(dir.c_str(), true);

  // Articles are written to SD as soon as each one finishes parsing, instead of
  // collecting them into feedData.articles first - some feeds run to dozens of
  // full-length posts, which doesn't fit in this device's heap if held all at
  // once (confirmed via crash reports even with per-field/article caps in
  // place). See RssParser.h.
  size_t articleIndex = 0;
  RssParser parser;
  parser.setArticleHandler([&dir, &articleIndex](const RssArticle& article) {
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
  // Straight into the first article - no folder listing detour. Article file
  // names are plain indices (0.txt, 1.txt, ...) not meant for manual browsing;
  // TxtReaderActivity's own paging (see NextBookFinder) moves between them, and
  // Back from inside an article returns here rather than to a file browser.
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
  activityManager.goToReader(firstArticle);
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
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, count] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), count);
    requestUpdate();
  });
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

  const int actionsHeight = actionRowCount() * metrics.listRowHeight;
  const int separatorGap = metrics.verticalSpacing;
  const int feedRegionHeight =
      std::max(metrics.listRowHeight, contentBottom - contentTop - separatorGap - actionsHeight);

  const bool feedRegionFocused = static_cast<int>(selectorIndex) < feedRegion;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, feedRegionHeight}, feedRegion,
      feedRegionFocused ? static_cast<int>(selectorIndex) : -1,
      [&feeds, hasFeeds](int index) -> std::string {
        if (!hasFeeds) return I18N.get(StrId::STR_RSS_NO_FEEDS);
        return feeds[index].title;
      },
      nullptr, [hasFeeds](int) { return hasFeeds ? UIIcon::Library : UIIcon::None; });

  const int separatorY = contentTop + feedRegionHeight + separatorGap / 2;
  renderer.drawLine(0, separatorY, pageWidth, separatorY);

  const int actionsTop = contentTop + feedRegionHeight + separatorGap;
  GUI.drawList(
      renderer, Rect{0, actionsTop, pageWidth, actionsHeight}, actionRowCount(),
      feedRegionFocused ? -1 : static_cast<int>(selectorIndex) - feedRegion,
      [](int index) -> std::string {
        switch (index) {
          case 0:
            return I18N.get(StrId::STR_RSS_ADD_FEED);
          case 1:
            return I18N.get(StrId::STR_RSS_SYNC_NOW);
          case 2:
            return I18N.get(StrId::STR_RSS_IMPORT_FROM_SD);
          default:
            return I18N.get(StrId::STR_RSS_MANAGE_FEEDS);
        }
      },
      nullptr, [](int) { return UIIcon::None; });

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

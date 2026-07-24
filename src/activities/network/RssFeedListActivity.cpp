#include "RssFeedListActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <RssParser.h>
#include <WiFi.h>

#include <cstdio>

#include "network/HttpDownloader.h"
#include "MappedInputManager.h"
#include "RssFeedStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
std::string rssFeedDir(size_t feedIndex) { return "/.picoread/rss/" + std::to_string(feedIndex); }
// Dropped onto the SD card root by the user on their PC; see sdcard/rss_feeds_import.json for the format.
constexpr const char* kImportFilePath = "/rss_feeds_import.json";
}  // namespace

int RssFeedListActivity::itemCount() const { return fixedRowCount() + static_cast<int>(RSS_STORE.getCount()); }

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
    requestUpdateAndWait();
    syncOneFeed(i);
  }
  state = State::List;
  requestUpdate();
}

bool RssFeedListActivity::syncOneFeed(size_t feedIndex) {
  const auto& feeds = RSS_STORE.getFeeds();
  if (feedIndex >= feeds.size()) return false;

  RssParser parser;
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

  const std::string dir = rssFeedDir(feedIndex);
  Storage.mkdir(dir.c_str(), true);

  for (size_t a = 0; a < parser.feedData.articles.size(); a++) {
    const auto& article = parser.feedData.articles[a];
    const std::string path = dir + "/" + std::to_string(a) + ".txt";
    HalFile file;
    if (!Storage.openFileForWrite("RSS", path, file)) continue;
    file.write(reinterpret_cast<const uint8_t*>(article.title.data()), article.title.size());
    file.write(reinterpret_cast<const uint8_t*>("\n\n"), 2);
    if (!article.link.empty()) {
      file.write(reinterpret_cast<const uint8_t*>(article.link.data()), article.link.size());
      file.write(reinterpret_cast<const uint8_t*>("\n\n"), 2);
    }
    file.write(reinterpret_cast<const uint8_t*>(article.description.data()), article.description.size());
  }
  return true;
}

void RssFeedListActivity::startImportFlow() {
  const size_t imported = RSS_STORE.importFromFile(kImportFilePath);
  char buf[64];
  snprintf(buf, sizeof(buf), tr(STR_RSS_IMPORT_RESULT_FORMAT), static_cast<int>(imported));
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_RSS_IMPORT_FROM_SD), buf),
                         [this](const ActivityResult&) { requestUpdate(); });
}

void RssFeedListActivity::onSelectFeed(size_t feedIndex) {
  const std::string dir = rssFeedDir(feedIndex);
  // Not synced yet (e.g. just imported from SD): the directory doesn't exist,
  // and FileBrowserActivity would silently fall back to the SD root, which
  // reads as "nothing happened" rather than "sync first".
  if (!Storage.exists(dir.c_str())) {
    startActivityForResult(
        std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_RSS_FEEDS), tr(STR_RSS_NOT_SYNCED_YET)),
        [this](const ActivityResult&) { requestUpdate(); });
    return;
  }
  activityManager.goToFileBrowser(dir);
}

void RssFeedListActivity::loop() {
  if (state != State::List) return;

  const int count = itemCount();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex == 0) {
      startAddFeedFlow();
    } else if (selectorIndex == 1) {
      startSyncFlow();
    } else if (selectorIndex == 2) {
      startImportFlow();
    } else {
      onSelectFeed(selectorIndex - fixedRowCount());
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
    GUI.drawPopup(renderer, busyMessage.c_str());
    renderer.displayBuffer();
    return;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const auto& feeds = RSS_STORE.getFeeds();

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount(), static_cast<int>(selectorIndex),
      [&feeds](int index) -> std::string {
        if (index == 0) return I18N.get(StrId::STR_RSS_ADD_FEED);
        if (index == 1) return I18N.get(StrId::STR_RSS_SYNC_NOW);
        if (index == 2) return I18N.get(StrId::STR_RSS_IMPORT_FROM_SD);
        return feeds[index - fixedRowCount()].title;
      },
      nullptr, [](int index) { return index < fixedRowCount() ? UIIcon::None : UIIcon::Library; });

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

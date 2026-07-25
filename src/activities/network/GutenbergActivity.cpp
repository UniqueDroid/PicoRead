#include "GutenbergActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>

#include "network/HttpDownloader.h"
#include "GutenbergJsonParser.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* kGutenbergDir = "/.picoread/gutenberg";
constexpr const char* kRandomBookPath = "/.picoread/gutenberg/random.epub";
constexpr const char* kManifestPath = "/.picoread/gutenberg/manifest.txt";

std::string popularBookPath(int index) {
  char buf[48];
  snprintf(buf, sizeof(buf), "/.picoread/gutenberg/popular%d.epub", index);
  return buf;
}

bool writeTextFile(const std::string& path, const std::string& content) {
  HalFile file;
  if (!Storage.openFileForWrite("GUTB", path, file)) return false;
  file.write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
  return true;
}
}  // namespace

void GutenbergActivity::onEnter() {
  Activity::onEnter();
  state = State::List;
  selectorIndex = 0;
  loadPopularTitles();
  requestUpdate();
}

void GutenbergActivity::onExit() {
  Activity::onExit();
  if (shouldTearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
  }
}

void GutenbergActivity::ensureWifiThen(const std::function<void()>& action) {
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

void GutenbergActivity::loadPopularTitles() {
  popularTitles.clear();
  HalFile file;
  if (!Storage.openFileForRead("GUTB", kManifestPath, file)) return;
  std::string line;
  char buf[256];
  size_t n;
  std::string pending;
  while ((n = file.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf))) > 0) {
    pending.append(buf, n);
  }
  size_t start = 0;
  while (start < pending.size() && popularTitles.size() < static_cast<size_t>(kPopularCount)) {
    const size_t nl = pending.find('\n', start);
    const std::string entry = nl == std::string::npos ? pending.substr(start) : pending.substr(start, nl - start);
    if (!entry.empty()) popularTitles.push_back(entry);
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
}

bool GutenbergActivity::downloadRandomBook() {
  // Gutendex has ~75k books at ~32/page; a bounded random page keeps this simple
  // without a separate call just to learn the exact current count.
  const int page = random(2000) + 1;
  char urlBuf[64];
  snprintf(urlBuf, sizeof(urlBuf), "https://gutendex.com/books/?page=%d", page);

  GutendexBooksParser parser(1);
  const bool fetchOk = HttpDownloader::fetchUrl(
      urlBuf, [&parser](const uint8_t* data, size_t len) {
        parser.feed(reinterpret_cast<const char*>(data), len);
        return true;
      });
  if (!fetchOk || parser.getBooks().empty() || parser.getBooks()[0].epubUrl.empty()) {
    LOG_ERR("GUTB", "Random book fetch failed (page %d)", page);
    return false;
  }

  Storage.mkdir(kGutenbergDir, true);
  return HttpDownloader::downloadToFile(parser.getBooks()[0].epubUrl, kRandomBookPath) == HttpDownloader::OK;
}

bool GutenbergActivity::downloadPopularBooks() {
  // Default sort is by download count descending - exactly "popular books".
  GutendexBooksParser parser(kPopularCount);
  const bool fetchOk = HttpDownloader::fetchUrl(
      "https://gutendex.com/books/", [&parser](const uint8_t* data, size_t len) {
        parser.feed(reinterpret_cast<const char*>(data), len);
        return true;
      });
  if (!fetchOk || parser.getBooks().empty()) {
    LOG_ERR("GUTB", "Popular books list fetch failed");
    return false;
  }

  Storage.mkdir(kGutenbergDir, true);
  std::string manifest;
  bool anyOk = false;
  const auto& books = parser.getBooks();
  for (size_t i = 0; i < books.size() && i < static_cast<size_t>(kPopularCount); i++) {
    manifest += books[i].title + "\n";
    if (books[i].epubUrl.empty()) continue;
    if (HttpDownloader::downloadToFile(books[i].epubUrl, popularBookPath(static_cast<int>(i))) ==
        HttpDownloader::OK) {
      anyOk = true;
    } else {
      LOG_ERR("GUTB", "Popular book %d download failed: %s", static_cast<int>(i), books[i].title.c_str());
    }
  }
  writeTextFile(kManifestPath, manifest);
  loadPopularTitles();
  return anyOk;
}

// Downloads everything in one pass, same reasoning as WikipediaActivity::syncAll -
// selecting an entry from the list never triggers a network fetch on its own, see
// openEpubOrPromptSync().
void GutenbergActivity::syncAll() {
  state = State::Busy;
  constexpr int kTotal = 2;
  int step = 0;

  auto showProgress = [&] {
    ++step;
    char buf[32];
    snprintf(buf, sizeof(buf), tr(STR_RSS_SYNCING_FORMAT), step, kTotal);
    busyMessage = buf;
    requestUpdateAndWait();
  };

  showProgress();
  downloadRandomBook();
  showProgress();
  downloadPopularBooks();

  state = State::List;
  requestUpdate();
}

void GutenbergActivity::promptSync() {
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_GUTENBERG), tr(STR_RSS_NOT_SYNCED_YET),
                                             tr(STR_CANCEL), tr(STR_RSS_SYNC_NOW_SHORT)),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          ensureWifiThen([this] { syncAll(); });
        } else {
          requestUpdate();
        }
      });
}

void GutenbergActivity::openEpubOrPromptSync(const std::string& path) {
  if (Storage.exists(path.c_str())) {
    activityManager.goToReader(path);
    return;
  }
  promptSync();
}

void GutenbergActivity::loop() {
  if (state != State::List) return;

  const int count = itemCount();
  const int contentCount = contentItemCount();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (static_cast<int>(selectorIndex) == 0) {
      openEpubOrPromptSync(kRandomBookPath);
    } else if (static_cast<int>(selectorIndex) < contentCount) {
      openEpubOrPromptSync(popularBookPath(static_cast<int>(selectorIndex) - 1));
    } else {
      ensureWifiThen([this] { syncAll(); });
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

void GutenbergActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_GUTENBERG));

  if (state == State::Busy) {
    GUI.drawPopup(renderer, busyMessage.c_str());
    renderer.displayBuffer();
    return;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentCount = contentItemCount();

  const int actionsHeight = actionItemCount() * metrics.listRowHeight;
  const int separatorGap = metrics.verticalSpacing;
  const int contentRegionHeight =
      std::max(metrics.listRowHeight, contentBottom - contentTop - separatorGap - actionsHeight);

  const bool contentFocused = static_cast<int>(selectorIndex) < contentCount;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentRegionHeight}, contentCount,
      contentFocused ? static_cast<int>(selectorIndex) : -1,
      [this](int index) -> std::string {
        if (index == 0) return I18N.get(StrId::STR_GUTENBERG_RANDOM_BOOK);
        const size_t popularIndex = static_cast<size_t>(index - 1);
        if (popularIndex < popularTitles.size()) return popularTitles[popularIndex];
        char buf[40];
        snprintf(buf, sizeof(buf), tr(STR_GUTENBERG_POPULAR_FORMAT), static_cast<int>(popularIndex) + 1);
        return buf;
      },
      nullptr, [](int) { return UIIcon::None; });

  const int separatorY = contentTop + contentRegionHeight + separatorGap / 2;
  renderer.drawLine(0, separatorY, pageWidth, separatorY);

  const int actionsTop = contentTop + contentRegionHeight + separatorGap;
  GUI.drawList(
      renderer, Rect{0, actionsTop, pageWidth, actionsHeight}, actionItemCount(),
      contentFocused ? -1 : static_cast<int>(selectorIndex) - contentCount,
      [](int) -> std::string { return I18N.get(StrId::STR_RSS_SYNC_NOW); }, nullptr,
      [](int) { return UIIcon::None; });

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

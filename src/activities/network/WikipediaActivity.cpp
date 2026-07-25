#include "WikipediaActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>

#include <algorithm>
#include <cstdio>

#include "network/HttpDownloader.h"
#include "MappedInputManager.h"
#include "WikipediaJsonParser.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* kWikiDir = "/.picoread/wikipedia";
constexpr const char* kArticlePath = "/.picoread/wikipedia/article.txt";
constexpr const char* kOnThisDayPath = "/.picoread/wikipedia/onthisday.txt";

std::string randomPath(int index) {
  char buf[48];
  snprintf(buf, sizeof(buf), "/.picoread/wikipedia/random%d.txt", index);
  return buf;
}

// LANGUAGE_CODES matches Wikipedia's own subdomain codes for most entries, but a
// couple of PicoRead's codes don't line up with Wikipedia's - "SI" here means
// Slovenian, but the ISO/Wikipedia code for Slovenian is "sl" ("si" is Sinhala);
// Valencian has no separate edition and is served by the Catalan Wikipedia.
std::string wikipediaLangCode() {
  const Language lang = I18N.getLanguage();
  if (lang == Language::SI) return "sl";
  if (lang == Language::CAV) return "ca";
  std::string code = LANGUAGE_CODES[static_cast<int>(lang)];
  std::transform(code.begin(), code.end(), code.begin(), ::tolower);
  return code;
}

std::string wikipediaApiBase() { return "https://" + wikipediaLangCode() + ".wikipedia.org/api/rest_v1"; }

// RTC read first (fast, no network if already synced); falls back to a direct NTP
// query if there's no RTC or it hasn't been set - independent of HalClock's
// syncFromNTP(), which requires RTC hardware to even attempt syncing and so can't
// help RTC-less devices. WiFi must already be connected.
bool getTodayDate(uint16_t& year, uint8_t& month, uint8_t& day) {
  if (halClock.getDate(year, month, day)) return true;

  configTzTime("UTC0", "pool.ntp.org", "time.nist.gov");
  for (int i = 0; i < 50; i++) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      const time_t now = time(nullptr);
      struct tm timeinfo;
      gmtime_r(&now, &timeinfo);
      year = 1900 + timeinfo.tm_year;
      month = timeinfo.tm_mon + 1;
      day = timeinfo.tm_mday;
      return true;
    }
    delay(100);
  }
  return false;
}

bool writeTextFile(const std::string& path, const std::string& content) {
  HalFile file;
  if (!Storage.openFileForWrite("WIKI", path, file)) return false;
  file.write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
  return true;
}
}  // namespace

void WikipediaActivity::onEnter() {
  Activity::onEnter();
  state = State::List;
  selectorIndex = 0;
  scroller.reset();
  requestUpdate();
}

void WikipediaActivity::onExit() {
  Activity::onExit();
  if (shouldTearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
  }
}

void WikipediaActivity::ensureWifiThen(const std::function<void()>& action) {
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

bool WikipediaActivity::downloadArticleOfDay() {
  uint16_t year;
  uint8_t month, day;
  if (!getTodayDate(year, month, day)) {
    LOG_ERR("WIKI", "Could not determine today's date");
    return false;
  }

  char urlBuf[160];
  snprintf(urlBuf, sizeof(urlBuf), "%s/feed/featured/%04u/%02u/%02u", wikipediaApiBase().c_str(), year, month, day);

  // Streamed straight into the parser: /feed/featured bundles tfa (what we want)
  // alongside mostread/news/onthisday, which can run to tens of KB combined -
  // buffering the whole response first risks the same OOM RssParser hit (see
  // WikipediaJsonParser.h).
  WikipediaFeaturedParser parser;
  const bool fetchOk = HttpDownloader::fetchUrl(
      urlBuf, [&parser](const uint8_t* data, size_t len) {
        parser.feed(reinterpret_cast<const char*>(data), len);
        return true;
      });
  if (!fetchOk) {
    LOG_ERR("WIKI", "Fetch failed: %s", urlBuf);
    return false;
  }

  if (parser.getArticleTitle().empty()) {
    LOG_ERR("WIKI", "No tfa.title in featured response");
    return false;
  }

  Storage.mkdir(kWikiDir, true);
  const std::string content = parser.getArticleTitle() + "\n\n" + parser.getArticleExtract();
  return writeTextFile(kArticlePath, content);
}

bool WikipediaActivity::downloadOnThisDay() {
  uint16_t year;
  uint8_t month, day;
  if (!getTodayDate(year, month, day)) {
    LOG_ERR("WIKI", "Could not determine today's date");
    return false;
  }

  char urlBuf[160];
  snprintf(urlBuf, sizeof(urlBuf), "%s/feed/onthisday/selected/%02u/%02u", wikipediaApiBase().c_str(), month, day);

  WikipediaOnThisDayParser parser;
  const bool fetchOk = HttpDownloader::fetchUrl(
      urlBuf, [&parser](const uint8_t* data, size_t len) {
        parser.feed(reinterpret_cast<const char*>(data), len);
        return true;
      });
  if (!fetchOk) {
    LOG_ERR("WIKI", "Fetch failed: %s", urlBuf);
    return false;
  }

  if (parser.getEventCount() == 0) {
    LOG_ERR("WIKI", "No events in on-this-day response");
    return false;
  }

  char headerBuf[48];
  snprintf(headerBuf, sizeof(headerBuf), tr(STR_WIKI_ON_THIS_DAY_FORMAT), static_cast<int>(month),
          static_cast<int>(day));
  const std::string content = std::string(headerBuf) + "\n\n" + parser.getDigest();

  Storage.mkdir(kWikiDir, true);
  return writeTextFile(kOnThisDayPath, content);
}

bool WikipediaActivity::downloadRandomArticle(int index) {
  const std::string url = wikipediaApiBase() + "/page/random/summary";

  WikipediaSummaryParser parser;
  const bool fetchOk = HttpDownloader::fetchUrl(
      url, [&parser](const uint8_t* data, size_t len) {
        parser.feed(reinterpret_cast<const char*>(data), len);
        return true;
      });
  if (!fetchOk) {
    LOG_ERR("WIKI", "Fetch failed: %s", url.c_str());
    return false;
  }

  if (parser.getTitle().empty()) {
    LOG_ERR("WIKI", "No title in random-article response");
    return false;
  }

  Storage.mkdir(kWikiDir, true);
  const std::string content = parser.getTitle() + "\n\n" + parser.getExtract();
  return writeTextFile(randomPath(index), content);
}

// Downloads all items in one pass so a device can be synced once (e.g. before
// leaving the house) and everything read offline afterward - selecting an
// entry from the list never triggers a network fetch on its own, see
// openTextOrPromptSync().
void WikipediaActivity::syncAll() {
  state = State::Busy;
  const int total = 2 + kRandomCount;
  int step = 0;

  auto showProgress = [&] {
    ++step;
    char buf[32];
    snprintf(buf, sizeof(buf), tr(STR_RSS_SYNCING_FORMAT), step, total);
    busyMessage = buf;
    busyProgressPercent = step * 100 / total;
    requestUpdateAndWait();
  };

  showProgress();
  downloadArticleOfDay();
  showProgress();
  downloadOnThisDay();
  for (int i = 0; i < kRandomCount; ++i) {
    showProgress();
    downloadRandomArticle(i);
  }

  busyProgressPercent = -1;
  state = State::List;
  requestUpdate();
}

void WikipediaActivity::promptSync() {
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_WIKIPEDIA), tr(STR_RSS_NOT_SYNCED_YET),
                                             tr(STR_CANCEL), tr(STR_RSS_SYNC_NOW_SHORT)),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          ensureWifiThen([this] { syncAll(); });
        } else {
          requestUpdate();
        }
      });
}

void WikipediaActivity::openTextOrPromptSync(const std::string& path) {
  if (Storage.exists(path.c_str())) {
    activityManager.goToReader(path);
    return;
  }
  promptSync();
}

void WikipediaActivity::openContentEntry(int index) {
  if (index == 0) {
    openTextOrPromptSync(kArticlePath);
  } else if (index == 1) {
    openTextOrPromptSync(kOnThisDayPath);
  } else {
    openTextOrPromptSync(randomPath(index - 2));
  }
}

std::string WikipediaActivity::contentLabelFor(int index) const {
  if (index == 0) return I18N.get(StrId::STR_WIKI_ARTICLE_OF_DAY);
  if (index == 1) return I18N.get(StrId::STR_WIKI_ON_THIS_DAY);
  char buf[48];
  snprintf(buf, sizeof(buf), "%s %d", I18N.get(StrId::STR_WIKI_RANDOM_ARTICLE), index - 1);
  return buf;
}

void WikipediaActivity::loop() {
  if (state != State::List) return;

  const int count = itemCount();
  const int contentCount = contentItemCount();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (static_cast<int>(selectorIndex) < contentCount) {
      openContentEntry(static_cast<int>(selectorIndex));
    } else {
      ensureWifiThen([this] { syncAll(); });
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

  if (static_cast<int>(selectorIndex) < contentCount) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int maxWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
    if (scroller.step(renderer, contentLabelFor(static_cast<int>(selectorIndex)), maxWidth)) requestUpdate(true);
  }
}

void WikipediaActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_WIKIPEDIA));

  if (state == State::Busy) {
    const Rect popupRect = GUI.drawPopup(renderer, busyMessage.c_str());
    if (busyProgressPercent >= 0) GUI.fillPopupProgress(renderer, popupRect, busyProgressPercent);
    return;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentCount = contentItemCount();

  const int bigRowHeight = ScrollingListRow::rowHeight(renderer);
  const int actionsHeight = actionItemCount() * bigRowHeight;
  const int separatorGap = metrics.verticalSpacing;
  const int contentRegionHeight = std::max(bigRowHeight, contentBottom - contentTop - separatorGap - actionsHeight);

  const bool contentFocused = static_cast<int>(selectorIndex) < contentCount;

  // Content region: Article of the Day, On This Day, then N random articles -
  // labelled "Random Article N" (1-indexed) since there's more than one now.
  const int contentPageItems = std::max(1, contentRegionHeight / bigRowHeight);
  const int contentPageStart =
      contentFocused ? static_cast<int>(selectorIndex) / contentPageItems * contentPageItems : 0;
  for (int i = contentPageStart; i < contentCount && i < contentPageStart + contentPageItems; i++) {
    const int rowY = contentTop + (i - contentPageStart) * bigRowHeight;
    const bool selected = contentFocused && i == static_cast<int>(selectorIndex);
    const std::string fullTitle = contentLabelFor(i);
    const std::string text = selected ? scroller.visibleText(fullTitle) : fullTitle;
    ScrollingListRow::draw(renderer, pageWidth, metrics.contentSidePadding, rowY, bigRowHeight, text, selected);
  }

  const int separatorY = contentTop + contentRegionHeight + separatorGap / 2;
  renderer.drawLine(0, separatorY, pageWidth, separatorY);

  const int actionsTop = contentTop + contentRegionHeight + separatorGap;
  ScrollingListRow::draw(renderer, pageWidth, metrics.contentSidePadding, actionsTop, bigRowHeight,
                        I18N.get(StrId::STR_RSS_SYNC_NOW), !contentFocused);

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

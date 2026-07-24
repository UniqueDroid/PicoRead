#include "WikipediaActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <I18n.h>
#include <JpegToBmpConverter.h>
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
#include "activities/util/BmpViewerActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* kWikiDir = "/.picoread/wikipedia";
constexpr const char* kArticlePath = "/.picoread/wikipedia/article.txt";
constexpr const char* kOnThisDayPath = "/.picoread/wikipedia/onthisday.txt";
constexpr const char* kRandomPath = "/.picoread/wikipedia/random.txt";
constexpr const char* kImageJpgPath = "/.picoread/wikipedia/potd.jpg";
constexpr const char* kImageBmpPath = "/.picoread/wikipedia/potd.bmp";

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

void WikipediaActivity::fetchArticleOfDay() {
  state = State::Busy;
  busyMessage = tr(STR_WIKI_LOADING);
  requestUpdateAndWait();

  uint16_t year;
  uint8_t month, day;
  if (!getTodayDate(year, month, day)) {
    LOG_ERR("WIKI", "Could not determine today's date");
    state = State::List;
    requestUpdate();
    return;
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
    state = State::List;
    requestUpdate();
    return;
  }

  if (parser.getArticleTitle().empty()) {
    LOG_ERR("WIKI", "No tfa.title in featured response");
    state = State::List;
    requestUpdate();
    return;
  }

  Storage.mkdir(kWikiDir, true);
  const std::string content = parser.getArticleTitle() + "\n\n" + parser.getArticleExtract();
  if (!writeTextFile(kArticlePath, content)) {
    state = State::List;
    requestUpdate();
    return;
  }

  activityManager.goToReader(kArticlePath);
}

void WikipediaActivity::fetchPictureOfDay() {
  state = State::Busy;
  busyMessage = tr(STR_WIKI_LOADING);
  requestUpdateAndWait();

  uint16_t year;
  uint8_t month, day;
  if (!getTodayDate(year, month, day)) {
    LOG_ERR("WIKI", "Could not determine today's date");
    state = State::List;
    requestUpdate();
    return;
  }

  char urlBuf[160];
  snprintf(urlBuf, sizeof(urlBuf), "%s/feed/featured/%04u/%02u/%02u", wikipediaApiBase().c_str(), year, month, day);

  WikipediaFeaturedParser parser;
  const bool fetchOk = HttpDownloader::fetchUrl(
      urlBuf, [&parser](const uint8_t* data, size_t len) {
        parser.feed(reinterpret_cast<const char*>(data), len);
        return true;
      });
  if (!fetchOk) {
    LOG_ERR("WIKI", "Fetch failed: %s", urlBuf);
    state = State::List;
    requestUpdate();
    return;
  }

  // getImageUrl() already prefers the thumbnail over the (potentially huge)
  // original - the display is ~800x480, no need to pull a multi-megapixel source
  // image over the air.
  const std::string imageUrl = parser.getImageUrl();
  if (imageUrl.empty()) {
    LOG_ERR("WIKI", "No image URL in featured response");
    state = State::List;
    requestUpdate();
    return;
  }

  Storage.mkdir(kWikiDir, true);
  if (HttpDownloader::downloadToFile(imageUrl, kImageJpgPath) != HttpDownloader::OK) {
    LOG_ERR("WIKI", "Image download failed: %s", imageUrl.c_str());
    state = State::List;
    requestUpdate();
    return;
  }

  HalFile jpegFile;
  HalFile bmpFile;
  if (!Storage.openFileForRead("WIKI", kImageJpgPath, jpegFile) ||
      !Storage.openFileForWrite("WIKI", kImageBmpPath, bmpFile)) {
    state = State::List;
    requestUpdate();
    return;
  }
  const bool converted = JpegToBmpConverter::jpegFileToBmpStreamWithSize(
      jpegFile, bmpFile, renderer.getScreenWidth(), renderer.getScreenHeight());
  if (!converted) {
    LOG_ERR("WIKI", "JPEG to BMP conversion failed");
    state = State::List;
    requestUpdate();
    return;
  }

  activityManager.replaceActivity(std::make_unique<BmpViewerActivity>(renderer, mappedInput, kImageBmpPath));
}

void WikipediaActivity::fetchOnThisDay() {
  state = State::Busy;
  busyMessage = tr(STR_WIKI_LOADING);
  requestUpdateAndWait();

  uint16_t year;
  uint8_t month, day;
  if (!getTodayDate(year, month, day)) {
    LOG_ERR("WIKI", "Could not determine today's date");
    state = State::List;
    requestUpdate();
    return;
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
    state = State::List;
    requestUpdate();
    return;
  }

  if (parser.getEventCount() == 0) {
    LOG_ERR("WIKI", "No events in on-this-day response");
    state = State::List;
    requestUpdate();
    return;
  }

  char headerBuf[48];
  snprintf(headerBuf, sizeof(headerBuf), tr(STR_WIKI_ON_THIS_DAY_FORMAT), static_cast<int>(month),
          static_cast<int>(day));
  const std::string content = std::string(headerBuf) + "\n\n" + parser.getDigest();

  Storage.mkdir(kWikiDir, true);
  if (!writeTextFile(kOnThisDayPath, content)) {
    state = State::List;
    requestUpdate();
    return;
  }

  activityManager.goToReader(kOnThisDayPath);
}

void WikipediaActivity::fetchRandomArticle() {
  state = State::Busy;
  busyMessage = tr(STR_WIKI_LOADING);
  requestUpdateAndWait();

  const std::string url = wikipediaApiBase() + "/page/random/summary";

  WikipediaSummaryParser parser;
  const bool fetchOk = HttpDownloader::fetchUrl(
      url, [&parser](const uint8_t* data, size_t len) {
        parser.feed(reinterpret_cast<const char*>(data), len);
        return true;
      });
  if (!fetchOk) {
    LOG_ERR("WIKI", "Fetch failed: %s", url.c_str());
    state = State::List;
    requestUpdate();
    return;
  }

  if (parser.getTitle().empty()) {
    LOG_ERR("WIKI", "No title in random-article response");
    state = State::List;
    requestUpdate();
    return;
  }

  Storage.mkdir(kWikiDir, true);
  const std::string content = parser.getTitle() + "\n\n" + parser.getExtract();
  if (!writeTextFile(kRandomPath, content)) {
    state = State::List;
    requestUpdate();
    return;
  }

  activityManager.goToReader(kRandomPath);
}

void WikipediaActivity::loop() {
  if (state != State::List) return;

  const int count = itemCount();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (selectorIndex) {
      case 0:
        ensureWifiThen([this] { fetchArticleOfDay(); });
        break;
      case 1:
        ensureWifiThen([this] { fetchPictureOfDay(); });
        break;
      case 2:
        ensureWifiThen([this] { fetchOnThisDay(); });
        break;
      case 3:
        ensureWifiThen([this] { fetchRandomArticle(); });
        break;
      default:
        break;
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

void WikipediaActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_WIKIPEDIA));

  if (state == State::Busy) {
    GUI.drawPopup(renderer, busyMessage.c_str());
    renderer.displayBuffer();
    return;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, itemCount(), static_cast<int>(selectorIndex),
      [](int index) -> std::string {
        switch (index) {
          case 0:
            return I18N.get(StrId::STR_WIKI_ARTICLE_OF_DAY);
          case 1:
            return I18N.get(StrId::STR_WIKI_PICTURE_OF_DAY);
          case 2:
            return I18N.get(StrId::STR_WIKI_ON_THIS_DAY);
          default:
            return I18N.get(StrId::STR_WIKI_RANDOM_ARTICLE);
        }
      },
      nullptr, [](int) { return UIIcon::None; });

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

#include "GutenbergActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>

#include "network/HttpDownloader.h"
#include "GutenbergManageActivity.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

using GutenbergPaths::popularBookPath;
using GutenbergPaths::writeTextFile;

namespace {
// Same pacing as the RSS lists - see RssArticleListActivity for why.
constexpr unsigned long kScrollStepMs = 480;
constexpr unsigned long kScrollPauseMs = 1200;

void drawBigRow(const GfxRenderer& renderer, int pageWidth, int sidePadding, int rowY, int rowHeight,
                const std::string& text, bool selected) {
  if (selected) renderer.fillRect(0, rowY, pageWidth, rowHeight);
  const int maxWidth = pageWidth - sidePadding * 2;
  const std::string truncated = renderer.truncatedText(UI_12_FONT_ID, text.c_str(), maxWidth);
  const int textY = rowY + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, sidePadding, textY, truncated.c_str(), !selected);
}
}  // namespace

std::string GutenbergActivity::contentLabelFor(int index) const {
  if (index == 0) return I18N.get(StrId::STR_GUTENBERG_RANDOM_BOOK);
  const size_t popularIndex = static_cast<size_t>(index - 1);
  if (popularIndex < popularBooks.size()) return popularBooks[popularIndex].title;
  char buf[40];
  snprintf(buf, sizeof(buf), tr(STR_GUTENBERG_POPULAR_FORMAT), static_cast<int>(popularIndex) + 1);
  return buf;
}

void GutenbergActivity::onEnter() {
  Activity::onEnter();
  state = State::List;
  selectorIndex = 0;

  // The list (titles + EPUB URLs) is small enough to just keep on SD - avoids
  // re-fetching every time the tile is revisited, unlike the actual EPUBs which
  // are only ever fetched on selection (see class comment in the header).
  popularBooks = GutenbergPaths::loadPopularBooksFromDisk();
  resetScroll();

  requestUpdate();
}

void GutenbergActivity::resetScroll() {
  scrollTitleOffset = 0;
  nextScrollStepMs = millis() + kScrollPauseMs;
}

void GutenbergActivity::stepScroll(const std::string& title) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int maxWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
  if (renderer.getTextWidth(UI_12_FONT_ID, title.c_str()) <= maxWidth) return;

  const unsigned long now = millis();
  if (now < nextScrollStepMs) return;

  size_t fitLen = 0;
  while (scrollTitleOffset + fitLen < title.size()) {
    const std::string sub = title.substr(scrollTitleOffset, fitLen + 1);
    if (renderer.getTextWidth(UI_12_FONT_ID, sub.c_str()) > maxWidth) break;
    fitLen++;
  }

  if (scrollTitleOffset + fitLen >= title.size()) {
    scrollTitleOffset = 0;
    nextScrollStepMs = now + kScrollPauseMs;
  } else {
    scrollTitleOffset++;
    nextScrollStepMs = now + kScrollStepMs;
  }
  requestUpdate(true);
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

// "Sync Now" - loads only the popular list's metadata (one small request), never
// the books themselves. An earlier version downloaded all 5 EPUBs right here,
// which could take several minutes with no feedback and looked like a freeze.
void GutenbergActivity::loadPopularList() {
  state = State::Busy;
  busyMessage = tr(STR_LOADING);
  requestUpdateAndWait();

  // Default sort is by download count descending - exactly "popular books".
  GutendexBooksParser parser(kPopularCount);
  const bool fetchOk = HttpDownloader::fetchUrl(
      "https://gutendex.com/books/", [&parser](const uint8_t* data, size_t len) {
        parser.feed(reinterpret_cast<const char*>(data), len);
        return true;
      });

  if (fetchOk && !parser.getBooks().empty()) {
    popularBooks = parser.getBooks();
    std::string listContent;
    for (const auto& book : popularBooks) {
      listContent += book.title + "\n" + book.epubUrl + "\n";
    }
    Storage.mkdir(GutenbergPaths::kDir, true);
    writeTextFile(GutenbergPaths::kListPath, listContent);
  } else {
    LOG_ERR("GUTB", "Popular books list fetch failed");
  }

  state = State::List;
  requestUpdate();
}

void GutenbergActivity::openRandomBook() {
  ensureWifiThen([this] {
    state = State::Busy;
    busyMessage = tr(STR_LOADING);
    requestUpdateAndWait();

    // Gutendex has ~75k books at ~32/page; a bounded random page keeps this
    // simple without a separate call just to learn the exact current count.
    const int page = random(2000) + 1;
    char urlBuf[64];
    snprintf(urlBuf, sizeof(urlBuf), "https://gutendex.com/books/?page=%d", page);

    GutendexBooksParser parser(1);
    const bool fetchOk = HttpDownloader::fetchUrl(
        urlBuf, [&parser](const uint8_t* data, size_t len) {
          parser.feed(reinterpret_cast<const char*>(data), len);
          return true;
        });

    bool ok = false;
    if (fetchOk && !parser.getBooks().empty() && !parser.getBooks()[0].epubUrl.empty()) {
      Storage.mkdir(GutenbergPaths::kDir, true);
      busyProgressPercent = 0;
      ok = HttpDownloader::downloadToFile(
               parser.getBooks()[0].epubUrl, GutenbergPaths::kRandomBookPath,
               [this](size_t downloaded, size_t total) {
                 busyProgressPercent = total > 0 ? static_cast<int>(downloaded * 100 / total) : 0;
                 requestUpdate(true);
               }) == HttpDownloader::OK;
    } else {
      LOG_ERR("GUTB", "Random book fetch failed (page %d)", page);
    }

    busyProgressPercent = -1;
    if (ok) {
      activityManager.goToReader(GutenbergPaths::kRandomBookPath);
    } else {
      state = State::List;
      requestUpdate();
    }
  });
}

void GutenbergActivity::openPopularBook(int index) {
  if (index < 0 || index >= static_cast<int>(popularBooks.size())) {
    promptLoadList();
    return;
  }

  const std::string path = popularBookPath(index);
  if (Storage.exists(path.c_str())) {
    activityManager.goToReader(path);
    return;
  }

  const std::string url = popularBooks[index].epubUrl;
  ensureWifiThen([this, path, url] {
    state = State::Busy;
    busyMessage = tr(STR_LOADING);
    requestUpdateAndWait();

    Storage.mkdir(GutenbergPaths::kDir, true);
    busyProgressPercent = 0;
    const bool ok = !url.empty() && HttpDownloader::downloadToFile(
                                        url, path,
                                        [this](size_t downloaded, size_t total) {
                                          busyProgressPercent = total > 0 ? static_cast<int>(downloaded * 100 / total) : 0;
                                          requestUpdate(true);
                                        }) == HttpDownloader::OK;
    busyProgressPercent = -1;
    if (ok) {
      activityManager.goToReader(path);
    } else {
      LOG_ERR("GUTB", "Popular book download failed: %s", path.c_str());
      state = State::List;
      requestUpdate();
    }
  });
}

void GutenbergActivity::promptLoadList() {
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_GUTENBERG), tr(STR_RSS_NOT_SYNCED_YET),
                                             tr(STR_CANCEL), tr(STR_RSS_SYNC_NOW_SHORT)),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          ensureWifiThen([this] { loadPopularList(); });
        } else {
          requestUpdate();
        }
      });
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
      openRandomBook();
    } else if (static_cast<int>(selectorIndex) < contentCount) {
      openPopularBook(static_cast<int>(selectorIndex) - 1);
    } else if (static_cast<int>(selectorIndex) == contentCount) {
      ensureWifiThen([this] { loadPopularList(); });
    } else {
      startActivityForResult(std::make_unique<GutenbergManageActivity>(renderer, mappedInput),
                             [this](const ActivityResult&) { requestUpdate(); });
    }
    return;
  }

  buttonNavigator.onNextRelease([this, count] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), count);
    resetScroll();
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, count] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), count);
    resetScroll();
    requestUpdate();
  });

  if (static_cast<int>(selectorIndex) < contentCount) {
    stepScroll(contentLabelFor(static_cast<int>(selectorIndex)));
  }
}

void GutenbergActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_GUTENBERG));

  if (state == State::Busy) {
    const Rect popupRect = GUI.drawPopup(renderer, busyMessage.c_str());
    if (busyProgressPercent >= 0) GUI.fillPopupProgress(renderer, popupRect, busyProgressPercent);
    return;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentCount = contentItemCount();

  const int bigRowHeight = renderer.getLineHeight(UI_12_FONT_ID) + 16;
  const int actionsHeight = actionItemCount() * bigRowHeight;
  const int separatorGap = metrics.verticalSpacing;
  const int contentRegionHeight = std::max(bigRowHeight, contentBottom - contentTop - separatorGap - actionsHeight);

  const bool contentFocused = static_cast<int>(selectorIndex) < contentCount;

  const int contentPageItems = std::max(1, contentRegionHeight / bigRowHeight);
  const int contentPageStart =
      contentFocused ? static_cast<int>(selectorIndex) / contentPageItems * contentPageItems : 0;
  for (int i = contentPageStart; i < contentCount && i < contentPageStart + contentPageItems; i++) {
    const int rowY = contentTop + (i - contentPageStart) * bigRowHeight;
    const bool selected = contentFocused && i == static_cast<int>(selectorIndex);
    const std::string fullTitle = contentLabelFor(i);
    const std::string text = (selected && scrollTitleOffset > 0 && scrollTitleOffset < fullTitle.size())
                                 ? fullTitle.substr(scrollTitleOffset)
                                 : fullTitle;
    drawBigRow(renderer, pageWidth, metrics.contentSidePadding, rowY, bigRowHeight, text, selected);
  }

  const int separatorY = contentTop + contentRegionHeight + separatorGap / 2;
  renderer.drawLine(0, separatorY, pageWidth, separatorY);

  const int actionsTop = contentTop + contentRegionHeight + separatorGap;
  for (int i = 0; i < actionItemCount(); i++) {
    const std::string label = i == 0 ? I18N.get(StrId::STR_RSS_SYNC_NOW) : I18N.get(StrId::STR_GUTENBERG_MANAGE_BOOKS);
    const int rowY = actionsTop + i * bigRowHeight;
    const bool selected = !contentFocused && i == static_cast<int>(selectorIndex) - contentCount;
    drawBigRow(renderer, pageWidth, metrics.contentSidePadding, rowY, bigRowHeight, label, selected);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

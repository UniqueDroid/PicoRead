#include "RssArticleListActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstring>

#include "MappedInputManager.h"
#include "RssFeedStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Fallback for feeds synced before index.txt existed: read the title (first
// line) directly out of each numbered article file instead. Slower (one open
// per article) but only needed once, until the feed is synced again.
std::string readFirstLine(const std::string& path) {
  HalFile file;
  if (!Storage.openFileForRead("RSSL", path, file)) return "";
  char buf[200];
  const size_t n = file.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf) - 1);
  buf[n] = '\0';
  const char* newline = strchr(buf, '\n');
  return newline ? std::string(buf, newline - buf) : std::string(buf, n);
}
}  // namespace

void RssArticleListActivity::loadTitles() {
  titles.clear();
  dir = RssFeedStore::articleDirFor(feedIndex);

  const std::string indexPath = dir + "/index.txt";
  HalFile indexFile;
  if (Storage.openFileForRead("RSSL", indexPath, indexFile)) {
    std::string content;
    char buf[256];
    size_t n;
    while ((n = indexFile.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf))) > 0) {
      content.append(buf, n);
    }
    size_t start = 0;
    while (start < content.size()) {
      const size_t nl = content.find('\n', start);
      const std::string line = nl == std::string::npos ? content.substr(start) : content.substr(start, nl - start);
      if (!line.empty()) titles.push_back(line);
      if (nl == std::string::npos) break;
      start = nl + 1;
    }
  }

  if (!titles.empty()) return;

  // No index.txt (feed synced before this existed) - fall back to reading each
  // article's own first line, same as Recents does for RSS-sourced entries.
  for (int i = 0; i < 200; i++) {
    const std::string path = dir + "/" + std::to_string(i) + ".txt";
    if (!Storage.exists(path.c_str())) break;
    const std::string title = readFirstLine(path);
    titles.push_back(title.empty() ? "?" : title);
  }
}

void RssArticleListActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  const auto& feeds = RSS_STORE.getFeeds();
  feedTitle = feedIndex < feeds.size() ? feeds[feedIndex].title : tr(STR_RSS_FEEDS);
  loadTitles();
  scroller.reset();
  requestUpdate();
}

void RssArticleListActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int count = static_cast<int>(titles.size());
  if (count == 0) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const std::string path = dir + "/" + std::to_string(selectorIndex) + ".txt";
    activityManager.goToReader(path);
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

  if (!titles.empty() && selectorIndex < titles.size()) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int maxWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
    if (scroller.step(renderer, titles[selectorIndex], maxWidth)) requestUpdate(true);
  }
}

void RssArticleListActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, feedTitle.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (titles.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_RSS_NOT_SYNCED_YET));
  } else {
    // Custom list rendering (not GUI.drawList) so this screen alone can use a
    // bigger font and marquee-scroll the selected row - drawList is shared by
    // every list screen in the app and hardcodes its own smaller font.
    const int rowHeight = ScrollingListRow::rowHeight(renderer);
    const int pageItems = std::max(1, contentHeight / rowHeight);
    const int itemCount = static_cast<int>(titles.size());
    const int pageStart = static_cast<int>(selectorIndex) / pageItems * pageItems;

    for (int i = pageStart; i < itemCount && i < pageStart + pageItems; i++) {
      const int rowY = contentTop + (i - pageStart) * rowHeight;
      const bool selected = i == static_cast<int>(selectorIndex);
      const std::string& fullTitle = titles[i];
      const std::string text = selected ? scroller.visibleText(fullTitle) : fullTitle;
      ScrollingListRow::draw(renderer, pageWidth, metrics.contentSidePadding, rowY, rowHeight, text, selected);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

#include "RssFeedManageActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "RssFeedStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

int RssFeedManageActivity::itemCount() const {
  const int feedCount = static_cast<int>(RSS_STORE.getCount());
  return feedCount == 0 ? 0 : feedCount + 1;  // +1 for the trailing "Delete All Feeds" row
}

void RssFeedManageActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  scroller.reset();
  requestUpdate();
}

void RssFeedManageActivity::deleteFeed(const size_t feedIndex) {
  if (feedIndex >= RSS_STORE.getCount()) return;

  // Folder names are derived from the feed's own title (see
  // RssFeedStore::articleDirFor), not its list position, so removing one feed
  // never disturbs any other feed's folder.
  Storage.removeDir(RssFeedStore::articleDirFor(feedIndex).c_str());
  RSS_STORE.removeFeed(feedIndex);

  if (selectorIndex > 0) selectorIndex--;
  scroller.reset();
  requestUpdate();
}

void RssFeedManageActivity::deleteAllFeeds() {
  Storage.removeDir("/.picoread/rss");
  RSS_STORE.clearAll();
  selectorIndex = 0;
  scroller.reset();
  requestUpdate();
}

void RssFeedManageActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int count = itemCount();

  if (count > 0 && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const auto feedCount = static_cast<size_t>(count - 1);
    if (selectorIndex < feedCount) {
      deleteFeed(selectorIndex);
    } else {
      deleteAllFeeds();
    }
    return;
  }

  if (count > 0) {
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

    const auto& feeds = RSS_STORE.getFeeds();
    if (static_cast<size_t>(selectorIndex) < feeds.size()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int maxWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;
      if (scroller.step(renderer, feeds[selectorIndex].title, maxWidth)) requestUpdate(true);
    }
  }
}

void RssFeedManageActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_RSS_MANAGE_FEEDS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int count = itemCount();

  if (count == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_RSS_NO_FEEDS));
  } else {
    const auto& feeds = RSS_STORE.getFeeds();
    const auto feedCount = feeds.size();
    const bool feedFocused = static_cast<size_t>(selectorIndex) < feedCount;

    const int bigRowHeight = ScrollingListRow::rowHeight(renderer);
    const int deleteAllRowHeight = bigRowHeight;
    const int separatorGap = metrics.verticalSpacing;
    const int feedListHeight = std::max(bigRowHeight, contentHeight - separatorGap - deleteAllRowHeight);

    const int feedPageItems = std::max(1, feedListHeight / bigRowHeight);
    const int feedPageStart = feedFocused ? static_cast<int>(selectorIndex) / feedPageItems * feedPageItems : 0;
    for (int i = feedPageStart; i < static_cast<int>(feedCount) && i < feedPageStart + feedPageItems; i++) {
      const int rowY = contentTop + (i - feedPageStart) * bigRowHeight;
      const bool selected = feedFocused && i == static_cast<int>(selectorIndex);
      const std::string& fullTitle = feeds[i].title;
      const std::string text = selected ? scroller.visibleText(fullTitle) : fullTitle;
      ScrollingListRow::draw(renderer, pageWidth, metrics.contentSidePadding, rowY, bigRowHeight, text, selected);
    }

    const int separatorY = contentTop + feedListHeight + separatorGap / 2;
    renderer.drawLine(0, separatorY, pageWidth, separatorY);

    const int deleteAllTop = contentTop + feedListHeight + separatorGap;
    ScrollingListRow::draw(renderer, pageWidth, metrics.contentSidePadding, deleteAllTop, deleteAllRowHeight,
                           I18N.get(StrId::STR_RSS_DELETE_ALL_FEEDS), !feedFocused);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

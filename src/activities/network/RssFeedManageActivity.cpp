#include "RssFeedManageActivity.h"

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
  requestUpdate();
}

void RssFeedManageActivity::deleteAllFeeds() {
  Storage.removeDir("/.picoread/rss");
  RSS_STORE.clearAll();
  selectorIndex = 0;
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
      requestUpdate();
    });
    buttonNavigator.onPreviousRelease([this, count] {
      selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), count);
      requestUpdate();
    });
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

    const int deleteAllRowHeight = metrics.listRowHeight;
    const int separatorGap = metrics.verticalSpacing;
    // Fit tightly to the actual feed count (so the divider sits right under the last
    // feed, not stretched to the bottom of the available area) but cap at the space
    // actually available so drawList's own pagination still kicks in for long lists.
    const int maxFeedListHeight = contentHeight - separatorGap - deleteAllRowHeight;
    const int feedListHeight =
        std::min(maxFeedListHeight, std::max(metrics.listRowHeight, static_cast<int>(feedCount) * metrics.listRowHeight));

    // Belt-and-braces: never let the delete-all row start past the bottom of the
    // content area, regardless of the math above - anchor it from the bottom too
    // and take whichever position is higher up (i.e. more conservative).
    const int deleteAllTopFromTop = contentTop + feedListHeight + separatorGap;
    const int deleteAllTopFromBottom = contentTop + contentHeight - deleteAllRowHeight;
    const int deleteAllTop = std::min(deleteAllTopFromTop, deleteAllTopFromBottom);
    const int separatorY = deleteAllTop - separatorGap / 2;

    LOG_DBG("RSS", "manage layout: contentTop=%d contentHeight=%d listRowHeight=%d feedCount=%d feedListHeight=%d deleteAllTop=%d",
            contentTop, contentHeight, metrics.listRowHeight, static_cast<int>(feedCount), feedListHeight, deleteAllTop);

    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, deleteAllTop - separatorGap - contentTop}, static_cast<int>(feedCount),
        feedFocused ? static_cast<int>(selectorIndex) : -1, [&feeds](int index) -> std::string { return feeds[index].title; },
        nullptr, [](int) { return UIIcon::Library; });

    renderer.drawLine(0, separatorY, pageWidth, separatorY);

    GUI.drawList(
        renderer, Rect{0, deleteAllTop, pageWidth, deleteAllRowHeight}, 1, feedFocused ? -1 : 0,
        [](int) -> std::string { return I18N.get(StrId::STR_RSS_DELETE_ALL_FEEDS); }, nullptr,
        [](int) { return UIIcon::None; });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

#include "RssFeedManageActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

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
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, count, static_cast<int>(selectorIndex),
        [&feeds, feedCount](int index) -> std::string {
          if (static_cast<size_t>(index) < feedCount) return feeds[index].title;
          return I18N.get(StrId::STR_RSS_DELETE_ALL_FEEDS);
        },
        nullptr, [feedCount](int index) { return static_cast<size_t>(index) < feedCount ? UIIcon::Library : UIIcon::None; });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

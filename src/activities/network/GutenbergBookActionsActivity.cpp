#include "GutenbergBookActionsActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include "GutenbergPaths.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Matches the other Gutenberg/RSS list screens' bigger font - see
// RssArticleListActivity for the full rationale. No marquee here: both labels
// are short, fixed strings that always fit.
void drawBigRow(const GfxRenderer& renderer, int pageWidth, int sidePadding, int rowY, int rowHeight,
                const std::string& text, bool selected) {
  if (selected) renderer.fillRect(0, rowY, pageWidth, rowHeight);
  const int maxWidth = pageWidth - sidePadding * 2;
  const std::string truncated = renderer.truncatedText(UI_12_FONT_ID, text.c_str(), maxWidth);
  const int textY = rowY + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, sidePadding, textY, truncated.c_str(), !selected);
}
}  // namespace

void GutenbergBookActionsActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  requestUpdate();
}

void GutenbergBookActionsActivity::moveToLibrary() {
  const std::string libraryPath = GutenbergPaths::loadLibraryPath();
  Storage.mkdir(libraryPath.c_str(), true);

  char sanitized[64];
  const std::string base = bookTitle.empty() ? "book" : bookTitle;
  FsHelpers::sanitizePathComponentForFat32(base.c_str(), sanitized, sizeof(sanitized));

  // Avoid clobbering an existing file of the same name in the user's library -
  // append " (2)", " (3)", ... until a free name is found.
  std::string destPath = libraryPath + "/" + sanitized + ".epub";
  int suffix = 2;
  while (Storage.exists(destPath.c_str())) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%s/%s (%d).epub", libraryPath.c_str(), sanitized, suffix++);
    destPath = buf;
  }

  if (!Storage.rename(bookPath.c_str(), destPath.c_str())) {
    LOG_ERR("GUTB", "Move to library failed: %s -> %s", bookPath.c_str(), destPath.c_str());
  }
  finish();
}

void GutenbergBookActionsActivity::deleteBook() {
  Storage.remove(bookPath.c_str());
  finish();
}

void GutenbergBookActionsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  constexpr int count = 2;  // Move to Library, Delete

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex == 0) {
      moveToLibrary();
    } else {
      deleteBook();
    }
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), count);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), count);
    requestUpdate();
  });
}

void GutenbergBookActionsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const std::string headerTitle = bookTitle.empty() ? I18N.get(StrId::STR_GUTENBERG_RANDOM_BOOK) : bookTitle;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, headerTitle.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  const int bigRowHeight = renderer.getLineHeight(UI_12_FONT_ID) + 16;
  for (int i = 0; i < 2; i++) {
    const std::string label = i == 0 ? I18N.get(StrId::STR_GUTENBERG_MOVE_TO_LIBRARY) : I18N.get(StrId::STR_DELETE);
    const int rowY = contentTop + i * bigRowHeight;
    drawBigRow(renderer, pageWidth, metrics.contentSidePadding, rowY, bigRowHeight, label,
              i == static_cast<int>(selectorIndex));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

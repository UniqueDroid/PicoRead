#include "GutenbergManageActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>

#include "GutenbergBookActionsActivity.h"
#include "GutenbergPaths.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void GutenbergManageActivity::loadBooks() {
  books.clear();

  if (Storage.exists(GutenbergPaths::kRandomBookPath)) {
    books.push_back({GutenbergPaths::kRandomBookPath, I18N.get(StrId::STR_GUTENBERG_RANDOM_BOOK)});
  }

  const auto popular = GutenbergPaths::loadPopularBooksFromDisk();
  for (int i = 0; i < GutenbergPaths::kPopularCount; i++) {
    const std::string path = GutenbergPaths::popularBookPath(i);
    if (!Storage.exists(path.c_str())) continue;

    std::string title = static_cast<size_t>(i) < popular.size() ? popular[i].title : std::string();
    if (title.empty()) {
      char buf[40];
      snprintf(buf, sizeof(buf), tr(STR_GUTENBERG_POPULAR_FORMAT), i + 1);
      title = buf;
    }
    books.push_back({path, title});
  }
}

void GutenbergManageActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  loadBooks();
  requestUpdate();
}

void GutenbergManageActivity::startChangeLibraryPathFlow() {
  const std::string current = GutenbergPaths::loadLibraryPath();
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_GUTENBERG_LIBRARY_PATH_LABEL), current,
                                              100, InputType::Text),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& kb = std::get<KeyboardResult>(result.data);
          if (!kb.text.empty()) GutenbergPaths::saveLibraryPath(kb.text);
        }
        requestUpdate();
      });
}

void GutenbergManageActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int count = itemCount();

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (static_cast<int>(selectorIndex) < bookCount()) {
      const auto book = books[selectorIndex];
      startActivityForResult(
          std::make_unique<GutenbergBookActionsActivity>(renderer, mappedInput, book.path, book.title),
          [this](const ActivityResult&) {
            loadBooks();
            if (selectorIndex > 0 && static_cast<int>(selectorIndex) >= itemCount()) selectorIndex--;
            requestUpdate();
          });
    } else {
      startChangeLibraryPathFlow();
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

void GutenbergManageActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_GUTENBERG_MANAGE_BOOKS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int count = itemCount();
  const int books_ = bookCount();

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, count, static_cast<int>(selectorIndex),
      [this, books_](int index) -> std::string {
        if (index < books_) return books[index].title;
        return I18N.get(StrId::STR_GUTENBERG_CHANGE_LIBRARY_PATH);
      },
      nullptr, [](int) { return UIIcon::None; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

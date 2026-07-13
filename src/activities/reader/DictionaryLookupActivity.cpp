#include "DictionaryLookupActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <utility>

#include "DictionaryIndex.h"
#include "DictionaryLibrary.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int LINE_HEIGHT = 34;
// Generous upper bound so wrappedText() never truncates with an ellipsis -
// scrolling handles definitions longer than the visible area instead.
constexpr int MAX_DEFINITION_LINES = 500;
}  // namespace

DictionaryLookupActivity::DictionaryLookupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                   std::vector<std::string> words, std::string dictionaryId)
    : Activity("DictionaryLookup", renderer, mappedInput), words(std::move(words)), dictionaryId(std::move(dictionaryId)) {}

void DictionaryLookupActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  mode = Mode::WordList;
  requestUpdate();
}

int DictionaryLookupActivity::getListHeight(const GfxRenderer& renderer) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return renderer.getScreenHeight() - metrics.topPadding - metrics.headerHeight - metrics.verticalSpacing -
         metrics.buttonHintsHeight - metrics.verticalSpacing;
}

void DictionaryLookupActivity::lookupSelectedWord() {
  if (words.empty() || selectorIndex >= words.size()) {
    return;
  }
  definitionWord = words[selectorIndex];

  DictionaryIndex index;
  std::string rawText;
  definitionFound = index.open(std::string(DictionaryLibrary::DICTIONARIES_DIR) + "/" + dictionaryId) &&
                    index.lookup(definitionWord, rawText);

  const std::string plainText = definitionFound ? stripHtml(rawText) : std::string(tr(STR_DEFINITION_NOT_FOUND));
  wrapDefinitionText(plainText);
  definitionScrollLine = 0;
  mode = Mode::Definition;
}

void DictionaryLookupActivity::wrapDefinitionText(const std::string& plainText) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int maxWidth = renderer.getScreenWidth() - metrics.contentSidePadding * 2;

  // wrappedText() wraps a single run of text; feed it one already-split
  // paragraph at a time so the newlines stripHtml() inserted at sense/
  // cross-reference boundaries survive as real line breaks.
  definitionLines.clear();
  size_t start = 0;
  while (start <= plainText.size()) {
    const size_t nl = plainText.find('\n', start);
    const std::string paragraph = plainText.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    if (!paragraph.empty()) {
      auto wrapped = renderer.wrappedText(UI_10_FONT_ID, paragraph.c_str(), maxWidth, MAX_DEFINITION_LINES);
      definitionLines.insert(definitionLines.end(), wrapped.begin(), wrapped.end());
    }
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
}

void DictionaryLookupActivity::loop() {
  if (mode == Mode::Definition) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      mode = Mode::WordList;
      requestUpdate();
      return;
    }
    const int visibleLines = getListHeight(renderer) / LINE_HEIGHT;
    const int maxScroll = std::max(0, static_cast<int>(definitionLines.size()) - visibleLines);
    buttonNavigator.onNext([this, maxScroll] {
      definitionScrollLine = std::min(definitionScrollLine + 1, maxScroll);
      requestUpdate();
    });
    buttonNavigator.onPrevious([this] {
      definitionScrollLine = std::max(definitionScrollLine - 1, 0);
      requestUpdate();
    });
    return;
  }

  // Mode::WordList
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!words.empty()) {
      lookupSelectedWord();
      requestUpdate();
    }
    return;
  }

  const int listSize = static_cast<int>(words.size());
  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });
  const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true);
  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void DictionaryLookupActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (mode == Mode::Definition) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, definitionWord.c_str());

    int y = contentTop;
    const int visibleLines = contentHeight / LINE_HEIGHT;
    for (int i = 0; i < visibleLines && definitionScrollLine + i < static_cast<int>(definitionLines.size()); i++) {
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, y, definitionLines[definitionScrollLine + i].c_str());
      y += LINE_HEIGHT;
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DICTIONARY));

    if (words.empty()) {
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_DICTIONARY_WORDS));
    } else {
      GUI.drawList(
          renderer, Rect{0, contentTop, pageWidth, contentHeight}, words.size(), selectorIndex,
          [this](int index) { return words[index]; });
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}

#pragma once
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Reader-menu "Dictionary" action: shows the distinct words on the current
// page as a list (mirrors EpubReaderFootnotesActivity - no per-word
// position data exists in the layout engine to drive a visual cursor, see
// lib/Dictionary/README), selecting one looks it up in the book-language-
// matched installed dictionary and shows the definition.
class DictionaryLookupActivity final : public Activity {
 public:
  // words: distinct words already extracted from the current page by the
  // caller (EpubReaderActivity, via Section::getTextFromSectionFile()).
  // dictionaryId: which /dictionaries/<id>/ folder to look up in - already
  // chosen by DictionaryLibrary::pickBestDictionaryId().
  explicit DictionaryLookupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                    std::vector<std::string> words, std::string dictionaryId);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Mode { WordList, Definition };

  std::vector<std::string> words;
  std::string dictionaryId;
  size_t selectorIndex = 0;
  ButtonNavigator buttonNavigator;
  Mode mode = Mode::WordList;

  std::string definitionWord;
  std::vector<std::string> definitionLines;  // word-wrapped for the content width
  int definitionScrollLine = 0;
  bool definitionFound = false;

  void lookupSelectedWord();
  void wrapDefinitionText(const std::string& plainText);
  int getListHeight(const GfxRenderer& renderer) const;
};

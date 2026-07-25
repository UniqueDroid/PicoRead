#pragma once
#include <string>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Reached by selecting a downloaded book in GutenbergManageActivity: a tiny
// 2-item menu (Move to Library / Delete) for that one book. Kept as its own
// screen rather than a 2-button ConfirmationActivity since there are three
// outcomes (move, delete, cancel via Back), not two.
class GutenbergBookActionsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  const std::string bookPath;
  const std::string bookTitle;

  void moveToLibrary();
  void deleteBook();

 public:
  GutenbergBookActionsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path,
                               std::string title)
      : Activity("GutenbergBookActions", renderer, mappedInput),
        bookPath(std::move(path)),
        bookTitle(std::move(title)) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

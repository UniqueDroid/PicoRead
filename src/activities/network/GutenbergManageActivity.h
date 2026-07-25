#pragma once
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

struct GutenbergDownloadedBook {
  std::string path;
  std::string title;
};

// "Manage Books" screen reached from the Gutenberg tile: lists downloaded EPUBs
// (Random Book + any Popular Book that's actually been fetched) plus a trailing
// "Change Library Folder" row. Selecting a book opens GutenbergBookActionsActivity
// (Move to Library / Delete); selecting the trailing row edits the destination
// folder "Move to Library" copies into (see GutenbergPaths::loadLibraryPath).
class GutenbergManageActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  std::vector<GutenbergDownloadedBook> books;

  void loadBooks();
  int bookCount() const { return static_cast<int>(books.size()); }
  int itemCount() const { return bookCount() + 1; }  // +1 for "Change Library Folder"
  void startChangeLibraryPathFlow();

 public:
  explicit GutenbergManageActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GutenbergManage", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

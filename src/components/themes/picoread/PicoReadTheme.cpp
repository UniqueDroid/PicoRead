#include "PicoReadTheme.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/book.h"
#include "components/icons/bookmark.h"
#include "components/icons/cover.h"
#include "components/icons/flappy.h"
#include "components/icons/folder.h"
#include "components/icons/hotspot.h"
#include "components/icons/library.h"
#include "components/icons/recent.h"
#include "components/icons/settings2.h"
#include "components/icons/transfer.h"
#include "components/icons/wifi.h"
#include "fontIds.h"

namespace {
constexpr int kColumns = 2;
constexpr int kGap = 12;
constexpr int kSidePadding = 16;
constexpr int kCoverHPadding = 8;
constexpr int kCoverCornerRadius = 6;
constexpr int kCornerRadius = 12;
constexpr int kIconSize = 32;
constexpr int kTileHeight = 92;

const uint8_t* iconForName(UIIcon icon) {
  switch (icon) {
    case UIIcon::Folder:
      return FolderIcon;
    case UIIcon::Book:
      return BookIcon;
    case UIIcon::Recent:
      return RecentIcon;
    case UIIcon::Settings:
      return Settings2Icon;
    case UIIcon::Transfer:
      return TransferIcon;
    case UIIcon::Library:
      return LibraryIcon;
    case UIIcon::Wifi:
      return WifiIcon;
    case UIIcon::Hotspot:
      return HotspotIcon;
    case UIIcon::Bookmark:
      return BookmarkIcon;
    case UIIcon::Flappy:
      return FlappyIcon;
    default:
      return nullptr;
  }
}
}  // namespace

void PicoReadTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                   const std::function<std::string(int index)>& buttonLabel,
                                   const std::function<UIIcon(int index)>& rowIcon) const {
  const int usableWidth = rect.width - 2 * kSidePadding;
  const int tileWidth = (usableWidth - kGap * (kColumns - 1)) / kColumns;
  const int tileHeight = kTileHeight;

  // rect.height as passed by HomeActivity doesn't account for homeCoverTileHeight
  // (see the same fix applied to BaseTheme/LyraTheme/RoundedRaffTheme's
  // drawButtonMenu/drawList - this bug isn't specific to one theme's math, it's in
  // what the caller passes), so derive real available space independently rather
  // than trust it here too.
  const int availableHeight = renderer.getScreenHeight() - rect.y - PicoReadMetrics::values.buttonHintsHeight;
  const int rowHeight = tileHeight + kGap;
  int visibleRows = availableHeight / rowHeight;
  if (visibleRows < 1) visibleRows = 1;

  const int totalRows = (buttonCount + kColumns - 1) / kColumns;
  const int selectedRow = std::max(0, selectedIndex) / kColumns;

  // Sliding window, not fixed pages: shows rows [0, visibleRows) until the
  // selection moves past the visible bottom edge, then the window follows by the
  // minimum needed - one row at a time, same continuous feel as the other themes'
  // list scrolling, no page jump and no separate arrow indicator needed.
  int windowStartRow = std::max(0, selectedRow - visibleRows + 1);
  windowStartRow = std::min(windowStartRow, std::max(0, totalRows - visibleRows));

  const int itemStart = windowStartRow * kColumns;
  const int itemEnd = std::min(buttonCount, itemStart + visibleRows * kColumns);

  for (int i = itemStart; i < itemEnd; ++i) {
    const int posInWindow = i - itemStart;
    const int col = posInWindow % kColumns;
    const int row = posInWindow / kColumns;
    const int tileX = rect.x + kSidePadding + col * (tileWidth + kGap);
    const int tileY = rect.y + row * (tileHeight + kGap);
    const bool selected = selectedIndex == i;

    if (selected) {
      renderer.fillRoundedRect(tileX, tileY, tileWidth, tileHeight, kCornerRadius, Color::LightGray);
    }
    renderer.drawRoundedRect(tileX, tileY, tileWidth, tileHeight, 2, kCornerRadius, true);

    const uint8_t* iconBitmap = rowIcon ? iconForName(rowIcon(i)) : nullptr;
    const int contentTop = tileY + (iconBitmap ? 12 : (tileHeight / 2 - 10));

    if (iconBitmap != nullptr) {
      renderer.drawIcon(iconBitmap, tileX + (tileWidth - kIconSize) / 2, contentTop, kIconSize);
    }

    const std::string label = buttonLabel(i);
    const std::string truncated = renderer.truncatedText(UI_10_FONT_ID, label.c_str(), tileWidth - 12);
    const int textWidth = renderer.getTextAdvanceX(UI_10_FONT_ID, truncated.c_str(), EpdFontFamily::REGULAR);
    const int textX = tileX + (tileWidth - textWidth) / 2;
    const int textY = iconBitmap != nullptr ? contentTop + kIconSize + 8 : contentTop;
    renderer.drawText(UI_10_FONT_ID, textX, textY, truncated.c_str());
  }
}

// Shows up to 3 recent books side by side instead of Classic's single cover
// (see PicoReadMetrics::homeRecentBooksCount). Title/author/"Continue Reading"
// are overlaid directly on the cover image (small box behind the text for
// legibility), matching Classic's single-cover style - not a separate label
// strip below the image.
void PicoReadTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                        const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                        bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const int count = PicoReadMetrics::values.homeRecentBooksCount;
  const int tileWidth = (rect.width - 2 * PicoReadMetrics::values.contentSidePadding) / count;
  const int tileY = rect.y;
  const int tileHeight = PicoReadMetrics::values.homeCoverTileHeight;
  const bool hasContinueReading = !recentBooks.empty();

  if (!hasContinueReading) {
    const int y = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID) - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_NO_OPEN_BOOK));
    renderer.drawCenteredText(UI_10_FONT_ID, y + renderer.getLineHeight(UI_12_FONT_ID), tr(STR_START_READING));
    return;
  }

  const int shown = std::min(static_cast<int>(recentBooks.size()), count);

  if (!coverRendered) {
    for (int i = 0; i < shown; i++) {
      const std::string coverPath = recentBooks[i].coverBmpPath;
      const int tileX = PicoReadMetrics::values.contentSidePadding + tileWidth * i;
      bool hasCover = false;
      if (!coverPath.empty()) {
        const std::string coverBmpPath = UITheme::getCoverThumbPath(coverPath, tileHeight);
        HalFile file;
        if (Storage.openFileForRead("HOME", coverBmpPath, file)) {
          Bitmap bitmap(file);
          if (bitmap.parseHeaders() == BmpReaderError::Ok) {
            renderer.drawBitmap(bitmap, tileX + kCoverHPadding, tileY, tileWidth - 2 * kCoverHPadding, tileHeight);
            hasCover = true;
          }
          file.close();
        }
      }
      renderer.drawRect(tileX + kCoverHPadding, tileY, tileWidth - 2 * kCoverHPadding, tileHeight, true);
      if (!hasCover) {
        renderer.fillRect(tileX + kCoverHPadding, tileY + tileHeight / 3, tileWidth - 2 * kCoverHPadding,
                          2 * tileHeight / 3, true);
        renderer.drawIcon(CoverIcon, tileX + kCoverHPadding + 24, tileY + 24, 32);
      }
    }
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  for (int i = 0; i < shown; i++) {
    const bool bookSelected = selectorIndex == i;
    const int tileX = PicoReadMetrics::values.contentSidePadding + tileWidth * i;
    const int maxTextWidth = tileWidth - 2 * kCoverHPadding - 16;

    const auto titleLines = renderer.wrappedText(UI_10_FONT_ID, recentBooks[i].title.c_str(), maxTextWidth, 2);
    const std::string& author = recentBooks[i].author;
    const std::string truncatedAuthor =
        author.empty() ? std::string{} : renderer.truncatedText(SMALL_FONT_ID, author.c_str(), maxTextWidth);

    const int titleLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
    int totalTextHeight = titleLineHeight * static_cast<int>(titleLines.size());
    if (!truncatedAuthor.empty()) totalTextHeight += renderer.getLineHeight(SMALL_FONT_ID) * 3 / 2;

    constexpr int kBottomMargin = 10;
    int titleY = tileY + tileHeight - totalTextHeight - kBottomMargin;

    if (coverRendered) {
      constexpr int boxPadding = 6;
      int maxLineWidth = 0;
      for (const auto& line : titleLines) {
        maxLineWidth = std::max(maxLineWidth, renderer.getTextWidth(UI_10_FONT_ID, line.c_str()));
      }
      if (!truncatedAuthor.empty()) {
        maxLineWidth = std::max(maxLineWidth, renderer.getTextWidth(SMALL_FONT_ID, truncatedAuthor.c_str()));
      }
      const int boxWidth = std::min(tileWidth - 2 * kCoverHPadding, maxLineWidth + boxPadding * 2);
      const int boxHeight = totalTextHeight + boxPadding * 2;
      const int boxX = tileX + (tileWidth - boxWidth) / 2;
      renderer.fillRect(boxX, titleY - boxPadding, boxWidth, boxHeight, bookSelected);
      renderer.drawRect(boxX, titleY - boxPadding, boxWidth, boxHeight, !bookSelected);
    }

    for (const auto& line : titleLines) {
      const int lineWidth = renderer.getTextWidth(UI_10_FONT_ID, line.c_str());
      renderer.drawText(UI_10_FONT_ID, tileX + (tileWidth - lineWidth) / 2, titleY, line.c_str(), !bookSelected);
      titleY += titleLineHeight;
    }
    if (!truncatedAuthor.empty()) {
      titleY += renderer.getLineHeight(SMALL_FONT_ID) / 2;
      const int authorWidth = renderer.getTextWidth(SMALL_FONT_ID, truncatedAuthor.c_str());
      renderer.drawText(SMALL_FONT_ID, tileX + (tileWidth - authorWidth) / 2, titleY, truncatedAuthor.c_str(),
                        !bookSelected);
    }
  }

  (void)bufferRestored;
}

// Same layout as BaseTheme::drawButtonHints, but with rounded top corners (bottom
// corners stay square since they sit flush against the screen edge).
void PicoReadTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                    const char* btn4) const {
  const GfxRenderer::Orientation origOrientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  const int pageHeight = renderer.getScreenHeight();
  constexpr int buttonWidth = 106;
  constexpr int buttonHeight = BaseMetrics::values.buttonHintsHeight;
  constexpr int buttonY = BaseMetrics::values.buttonHintsHeight;
  constexpr int textYOffset = 7;
  constexpr int hintCornerRadius = 8;
  constexpr int x4ButtonPositions[] = {25, 130, 245, 350};
  constexpr int x3ButtonPositions[] = {38, 154, 268, 384};
  const int* buttonPositions = gpio.deviceIsX3() ? x3ButtonPositions : x4ButtonPositions;
  const char* labels[] = {btn1, btn2, btn3, btn4};

  for (int i = 0; i < 4; i++) {
    if (labels[i] != nullptr && labels[i][0] != '\0') {
      const int x = buttonPositions[i];
      const int y = pageHeight - buttonY;
      renderer.fillRoundedRect(x, y, buttonWidth, buttonHeight, hintCornerRadius, true, true, false, false,
                               Color::White);
      renderer.drawRoundedRect(x, y, buttonWidth, buttonHeight, 2, hintCornerRadius, true, true, false, false, true);
      const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, labels[i]);
      const int textX = x + (buttonWidth - 1 - textWidth) / 2;
      renderer.drawText(UI_10_FONT_ID, textX, y + textYOffset, labels[i]);
    }
  }

  renderer.setOrientation(origOrientation);
}

#include "PicoReadTheme.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

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

  for (int i = 0; i < buttonCount; ++i) {
    const int col = i % kColumns;
    const int row = i / kColumns;
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
// (see PicoReadMetrics::homeRecentBooksCount). Adapted from Lyra3CoversTheme,
// the existing 3-cover reference implementation in this codebase.
void PicoReadTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                        const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                        bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const int count = PicoReadMetrics::values.homeRecentBooksCount;
  const int tileWidth = (rect.width - 2 * PicoReadMetrics::values.contentSidePadding) / count;
  const int tileY = rect.y;
  const bool hasContinueReading = !recentBooks.empty();

  if (!hasContinueReading) {
    const int y = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID) - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_NO_OPEN_BOOK));
    renderer.drawCenteredText(UI_10_FONT_ID, y + renderer.getLineHeight(UI_12_FONT_ID), tr(STR_START_READING));
    return;
  }

  if (!coverRendered) {
    for (int i = 0; i < std::min(static_cast<int>(recentBooks.size()), count); i++) {
      const std::string coverPath = recentBooks[i].coverBmpPath;
      bool hasCover = true;
      const int tileX = PicoReadMetrics::values.contentSidePadding + tileWidth * i;
      if (coverPath.empty()) {
        hasCover = false;
      } else {
        const std::string coverBmpPath = UITheme::getCoverThumbPath(coverPath, PicoReadMetrics::values.homeCoverHeight);
        HalFile file;
        if (Storage.openFileForRead("HOME", coverBmpPath, file)) {
          Bitmap bitmap(file);
          if (bitmap.parseHeaders() == BmpReaderError::Ok) {
            const float coverHeight = static_cast<float>(bitmap.getHeight());
            const float coverWidth = static_cast<float>(bitmap.getWidth());
            const float ratio = coverWidth / coverHeight;
            const float tileRatio = static_cast<float>(tileWidth - 2 * kCoverHPadding) /
                                    static_cast<float>(PicoReadMetrics::values.homeCoverHeight);
            const float cropX = 1.0f - (tileRatio / ratio);
            renderer.drawBitmap(bitmap, tileX + kCoverHPadding, tileY + kCoverHPadding, tileWidth - 2 * kCoverHPadding,
                                PicoReadMetrics::values.homeCoverHeight, cropX);
          } else {
            hasCover = false;
          }
          file.close();
        }
      }
      renderer.drawRect(tileX + kCoverHPadding, tileY + kCoverHPadding, tileWidth - 2 * kCoverHPadding,
                        PicoReadMetrics::values.homeCoverHeight, true);
      if (!hasCover) {
        renderer.fillRect(tileX + kCoverHPadding, tileY + kCoverHPadding + (PicoReadMetrics::values.homeCoverHeight / 3),
                          tileWidth - 2 * kCoverHPadding, 2 * PicoReadMetrics::values.homeCoverHeight / 3, true);
        renderer.drawIcon(CoverIcon, tileX + kCoverHPadding + 24, tileY + kCoverHPadding + 24, 32);
      }
    }
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  for (int i = 0; i < std::min(static_cast<int>(recentBooks.size()), count); i++) {
    const bool bookSelected = selectorIndex == i;
    const int tileX = PicoReadMetrics::values.contentSidePadding + tileWidth * i;
    const int maxLineWidth = tileWidth - 2 * kCoverHPadding;
    const auto titleLines = renderer.wrappedText(SMALL_FONT_ID, recentBooks[i].title.c_str(), maxLineWidth, 3);
    const int titleLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
    const int dynamicBlockHeight = static_cast<int>(titleLines.size()) * titleLineHeight;
    const int dynamicTitleBoxHeight = dynamicBlockHeight + kCoverHPadding + 5;

    if (bookSelected) {
      renderer.fillRoundedRect(tileX, tileY, tileWidth, kCoverHPadding, kCoverCornerRadius, true, true, false, false,
                               Color::LightGray);
      renderer.fillRectDither(tileX, tileY + kCoverHPadding, kCoverHPadding, PicoReadMetrics::values.homeCoverHeight,
                              Color::LightGray);
      renderer.fillRectDither(tileX + tileWidth - kCoverHPadding, tileY + kCoverHPadding, kCoverHPadding,
                              PicoReadMetrics::values.homeCoverHeight, Color::LightGray);
      renderer.fillRoundedRect(tileX, tileY + PicoReadMetrics::values.homeCoverHeight + kCoverHPadding, tileWidth,
                               dynamicTitleBoxHeight, kCoverCornerRadius, false, false, true, true, Color::LightGray);
    }

    int currentY = tileY + PicoReadMetrics::values.homeCoverHeight + kCoverHPadding + 5;
    for (const auto& line : titleLines) {
      renderer.drawText(SMALL_FONT_ID, tileX + kCoverHPadding, currentY, line.c_str(), true);
      currentY += titleLineHeight;
    }
  }

  (void)bufferRestored;
}

#include "PicoReadTheme.h"

#include <GfxRenderer.h>

#include "components/icons/book.h"
#include "components/icons/bookmark.h"
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
constexpr int kCornerRadius = 12;
constexpr int kIconSize = 32;
constexpr int kTileHeight = 78;

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

#include "HomeMenuCustomizeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "HomeMenuLayoutStore.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void HomeMenuCustomizeActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void HomeMenuCustomizeActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int count = static_cast<int>(HOME_MENU_LAYOUT.getEntries().size());
  if (count == 0) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const auto& entries = HOME_MENU_LAYOUT.getEntries();
    HOME_MENU_LAYOUT.setVisible(static_cast<size_t>(selectedIndex), !entries[selectedIndex].visible);
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (selectedIndex > 0) {
      HOME_MENU_LAYOUT.moveUp(static_cast<size_t>(selectedIndex));
      selectedIndex--;
      requestUpdate();
    }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (selectedIndex + 1 < count) {
      HOME_MENU_LAYOUT.moveDown(static_cast<size_t>(selectedIndex));
      selectedIndex++;
      requestUpdate();
    }
    return;
  }

  buttonNavigator.wireListNav(selectedIndex, count, count, [this] { requestUpdate(); });
}

void HomeMenuCustomizeActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CUSTOMIZE_HOME_MENU));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const auto& entries = HOME_MENU_LAYOUT.getEntries();
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(entries.size()), selectedIndex,
      [&entries](int index) { return std::string(homeMenuItemLabel(entries[index].item)); }, nullptr,
      [&entries](int index) { return homeMenuItemIcon(entries[index].item); },
      [&entries](int index) -> std::string { return entries[index].visible ? tr(STR_SHOW) : tr(STR_HIDE); }, false,
      [&entries](int index) { return !entries[index].visible; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TOGGLE), tr(STR_MOVE_UP), tr(STR_MOVE_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

#pragma once

#include <I18n.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class ReaderQuickSettingsActivity final : public Activity {
  enum class Item : uint8_t {
    ColorMode,
#if CROSSPOINT_PAPERS3
    ExternalFont,
#endif
    FontSize,
    LineSpacing,
    ScreenMargin,
    ParagraphAlignment,
    Orientation,
    FirstLineIndent,
    TextAntiAliasing,
    Images,
    ItemCount,
  };

  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool changed = false;

  static constexpr int itemCount() { return static_cast<int>(Item::ItemCount); }
  static Item itemAt(int index) { return static_cast<Item>(index); }

  StrId labelFor(Item item) const;
  std::string valueFor(Item item) const;
  void activateSelected();
  void adjustSelected(int delta);
  void finishWithResult(bool cancelled);
  void markChanged();

 public:
  explicit ReaderQuickSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReaderQuickSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};

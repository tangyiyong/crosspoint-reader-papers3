#pragma once

#include "activities/Activity.h"

class FontPreviewActivity final : public Activity {
 public:
  FontPreviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int fontIndex)
      : Activity("FontPreview", renderer, mappedInput), fontIndex(fontIndex) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int fontIndex = -1;
  int originalReaderFontIndex = -1;
  int originalUiFontIndex = -1;
  bool previewInstalled = false;
  bool applied = false;

  bool canPreviewSelectedFont() const;
  void restorePreviewFont();
};

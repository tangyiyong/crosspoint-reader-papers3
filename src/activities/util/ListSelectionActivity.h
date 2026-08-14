#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct Rect;

class ListSelectionActivity final : public Activity {
  std::string title;
  std::vector<std::string> items;
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  Rect listRect() const;

 public:
  ListSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title,
                        std::vector<std::string> items, int initialIndex = 0);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

#pragma once

#include <string>

#include "activities/Activity.h"

class NotepadActivity final : public Activity {
  static constexpr size_t NOTE_MAX_BYTES = 4096;
  std::string note;
  bool loaded = false;

  void loadNote();
  void saveNote(const std::string& text);
  void editNote();

 public:
  explicit NotepadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Notepad", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

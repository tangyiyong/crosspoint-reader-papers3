#include "NotepadActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <variant>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char NOTE_PATH[] = "/.crosspoint/notepad.txt";
}

void NotepadActivity::onEnter() {
  Activity::onEnter();
  loadNote();
  requestUpdate();
}

void NotepadActivity::loadNote() {
  loaded = true;
  note.clear();
  if (!Storage.exists(NOTE_PATH)) {
    return;
  }
  const String text = Storage.readFile(NOTE_PATH);
  note.assign(text.c_str(), std::min(static_cast<size_t>(text.length()), NOTE_MAX_BYTES));
}

void NotepadActivity::saveNote(const std::string& text) {
  Storage.mkdir("/.crosspoint");
  note = text.substr(0, NOTE_MAX_BYTES);
  Storage.writeFile(NOTE_PATH, String(note.c_str()));
}

void NotepadActivity::editNote() {
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_APP_NOTEPAD), note, 512, false),
                         [this](const ActivityResult& result) {
                           if (!result.isCancelled && std::holds_alternative<KeyboardResult>(result.data)) {
                             saveNote(std::get<KeyboardResult>(result.data).text);
                           }
                           requestUpdate();
                         });
}

void NotepadActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasContentTapped()) {
    editNote();
  }
}

void NotepadActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentX = metrics.contentSidePadding;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_NOTEPAD));

  const char* text = note.empty() ? tr(STR_NOTEPAD_EMPTY) : note.c_str();
  const int maxLines = std::max(1, (pageHeight - y - metrics.buttonHintsHeight - 16) / (renderer.getLineHeight(UI_10_FONT_ID) + 4));
  const auto lines = renderer.wrappedText(UI_10_FONT_ID, text, contentW, maxLines);
  for (const auto& line : lines) {
    renderer.drawText(UI_10_FONT_ID, contentX, y, line.c_str());
    y += renderer.getLineHeight(UI_10_FONT_ID) + 4;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_EDIT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

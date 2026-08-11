# Touch Menu Direct Selection

Date: 2026-08-10
Branch: codex/touch-menu-direct-selection-2026-08-10

## Background

M5Stack PaperS3 uses a GT911 capacitive touch controller. The current UI can be operated through the footer Back / Select / Up / Down touch buttons, but most menu and list screens do not allow tapping the visible option itself.

## Requirements

- Add phone-like direct tap selection for menu and list screens.
- Preserve the existing footer button workflow for Back / Select / Up / Down.
- A content-area tap on a visible list row should select and activate that row.
- A content-area tap on a visible home menu tile should select and activate that tile.
- Hit testing must use the same row height, tile height, spacing, and paging rules used by the active UI theme.
- The change must not alter reader page-turn gestures or reader menu gestures.
- The implementation must avoid new heap allocations in the touch hot path.
- Existing long-press actions, including file deletion in the browser, remain on the existing footer Select path.
- Add PaperS3 edge gestures inspired by common e-reader / mobile navigation:
  - Left edge swipe right returns one level.
  - Right edge swipe left advances to the next reader page or next list page.
  - Top edge swipe down reveals a reader Back button; tapping the top button returns one level.
  - Bottom edge swipe up opens the reader menu while reading; on list screens it keeps scrolling to the next list page.
- Keep reader tap zones, two-finger Back, and long-press menu shortcuts working.

## Feasibility

Feasible. The UI already has logical touch coordinates exposed through `MappedInputManager`, and list rendering is centralized in the theme layer. The main blocker is that non-reader activities enable footer mode, and `HalGPIO` currently suppresses content-area taps while footer mode is active. A small input-layer event can expose content taps without changing footer button mapping.

## Task List

- [x] Create a dedicated feature branch.
- [x] Document feature requirements and feasibility.
- [x] Add content-area tap event for footer-mode screens.
- [x] Add shared list and button-menu hit testing helpers.
- [x] Enable direct tapping on primary navigation screens.
- [x] Enable direct tapping on settings and picker screens.
- [x] Enable direct tapping on network / OPDS / firmware list screens where safe.
- [x] Enable direct tapping on reader menu / chapter / footnote selection screens.
- [x] Build and review changed files.
- [x] Add content-area swipe gestures for paged list navigation.
- [x] Enable swipe-up for next page and swipe-down for previous page in scrollable list screens.
- [x] Verify the swipe paging build.
- [x] Create a dedicated branch for PaperS3 edge gestures.
- [x] Add edge Back and edge Next semantic helpers.
- [x] Recognize left-edge swipe right as an immediate Back gesture.
- [x] Recognize top-edge swipe down as a reader Back button reveal gesture.
- [x] Add a lightweight reader Back overlay for EPUB, TXT, and XTC pages.
- [x] Recognize right-edge swipe left as next-page/next-list-page gesture.
- [x] Keep bottom-edge swipe up as reader menu shortcut without breaking list scrolling.
- [x] Build and verify the edge gesture changes.
- [x] Expand the reader top gesture overlay into a larger Back / Home / Settings quick bar.
- [x] Add a dedicated reader quick settings panel for bottom-edge swipe-up.
- [x] Rebuild reader layout after quick settings change text layout or display options.
- [x] Add USB Mass Storage as an option in the File Transfer menu.
- [x] Verify USB Mass Storage build enablement against the current Arduino/TinyUSB path.
- [x] Build and verify the quick gesture/settings/USB changes.
- [x] Prevent reader edge gestures from also firing reader zone buttons while the finger is held.
- [x] Expand the top reader quick bar hit area to full screen width.
- [x] Align reader quick settings tap hit-testing with its rendered row height.

## Implementation Log

- 2026-08-10: Created branch `codex/touch-menu-direct-selection-2026-08-10`.
- 2026-08-10: Confirmed menu/list screens mostly render through `BaseTheme::drawList()` / Lyra override, so one shared hit-test helper can match the drawn paging window.
- 2026-08-10: Added `HalGPIO::wasContentTapReleased()` / `MappedInputManager::wasContentTapped()` for footer-mode content taps without changing footer button behavior.
- 2026-08-10: Added `UITheme::hitTestListItem()` and `UITheme::hitTestButtonMenu()` to centralize row/tile hit testing with current theme metrics.
- 2026-08-10: Enabled content tap activation in Home, file browser, recent books, Settings, language selection, external font selection, button remapping, network mode selection, WiFi network selection, Calibre settings, KOReader settings, status bar settings, OPDS server/settings screens, SD firmware selection, reader menu, EPUB/XTC chapter selection, and EPUB footnotes.
- 2026-08-10: `pio run` completed successfully. `clang-format` was not available on this host, so no automatic formatting pass was applied.
- 2026-08-11: Started content-area swipe paging. Swipe-up should advance the visible list page; swipe-down should return to the previous list page. Footer buttons keep their existing row-by-row behavior.
- 2026-08-11: Added footer-mode content swipe events in `HalGPIO` / `MappedInputManager`, keeping tap and footer button events separate.
- 2026-08-11: Connected swipe-up/down paging to file browser, recent books, settings lists, WiFi networks, OPDS lists, SD firmware selection, EPUB/XTC chapter pickers, EPUB footnotes, and OPDS book browser.
- 2026-08-11: `pio run` completed successfully after adding the missing `ButtonNavigator` include for font selection.
- 2026-08-12: Created branch `codex/papers3-edge-gestures-2026-08-12` from the current touch branch.
- 2026-08-12: Added `MappedInputManager::wasEdgeBackGesture()` and `wasEdgeNextGesture()` so pages can consume high-level edge semantics instead of raw touch coordinates.
- 2026-08-12: Extended footer-mode content gesture classification in `HalGPIO` for left-edge right swipe, right-edge left swipe, top-edge down swipe, and bottom-edge up swipe.
- 2026-08-12: Mapped left-edge right swipe into logical Back so existing page-level Back handling returns one level across file browser, settings, network, OPDS, and picker screens.
- 2026-08-12: Changed reader top-edge down swipe to reveal a small top Back button. Tapping the revealed top button returns from EPUB/TXT/XTC reading to the file browser at the current book folder.
- 2026-08-12: Mapped right-edge left swipe into content swipe-up semantics so scrollable list pages advance to the next visible page; reader pages already use the same raw swipe-left event for next page.
- 2026-08-12: Preserved bottom-edge swipe-up as the reader menu shortcut while also allowing list pages to treat the same motion as next-page scrolling.
- 2026-08-12: `git diff --check` passed.
- 2026-08-12: `pio run` completed successfully for the `default` environment.
- 2026-08-12: Started second gesture iteration: larger top quick bar, richer bottom reader settings, and USB Mass Storage entry in File Transfer.
- 2026-08-12: Reviewed `zty012/zc`: it uses ESP-IDF TinyUSB MSC with `tinyusb_msc_new_storage_sdmmc()` to expose an SD card when booting into USB mode. Current PaperS3 port already has an Arduino `USBMSC` + SdFat sector callback service, so the safer migration path is wiring the existing service into the UI instead of replacing the storage stack.
- 2026-08-12: Expanded the reader top-edge reveal from one small Back button to a larger three-button quick bar: Back, Home, and Settings.
- 2026-08-12: Added `ReaderQuickSettingsActivity` for bottom-edge swipe-up and top quick-bar Settings. It exposes color mode, external font, font size, line spacing, screen margin, paragraph alignment, orientation, first-line indent, text anti-aliasing, and EPUB image rendering mode.
- 2026-08-12: Wired EPUB/TXT/XTC readers to reopen layout/rendering after quick settings changes. EPUB clears the active `Section`, TXT clears the streaming page index, and XTC refreshes display/orientation state.
- 2026-08-12: Added USB Mass Storage as a fourth File Transfer mode and routed it to `UsbMassStorageActivity`.
- 2026-08-12: Added `STR_USB_MSC_DESC` to English and Simplified Chinese translations; other languages fall back to English through the existing generator.
- 2026-08-12: `git diff --check` passed.
- 2026-08-12: `pio run` completed successfully for the `default` environment. The build includes Arduino `USBMSC.cpp`, confirming the current `ARDUINO_USB_MODE=0` + `CONFIG_TINYUSB_MSC_ENABLED=1` path is compiled.
- 2026-08-12: Fixed duplicate reader actions from edge gestures. In reader mode, touches starting from the screen edge no longer publish their left/right/confirm zone button while held, so a right-edge swipe turns one page only, left-edge swipe returns directly, and bottom-edge swipe opens quick settings without first turning a page.
- 2026-08-12: Expanded the top quick bar hit-test and drawing to the full screen width split into three equal Back / Home / Settings zones.
- 2026-08-12: Fixed reader quick settings tap offset. The panel renders single-line rows with right-side values, but tap hit-testing was using the taller subtitle row metric, causing lower rows such as Images to activate the row above.

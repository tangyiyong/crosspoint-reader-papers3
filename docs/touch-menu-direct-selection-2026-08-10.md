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

## Implementation Log

- 2026-08-10: Created branch `codex/touch-menu-direct-selection-2026-08-10`.
- 2026-08-10: Confirmed menu/list screens mostly render through `BaseTheme::drawList()` / Lyra override, so one shared hit-test helper can match the drawn paging window.
- 2026-08-10: Added `HalGPIO::wasContentTapReleased()` / `MappedInputManager::wasContentTapped()` for footer-mode content taps without changing footer button behavior.
- 2026-08-10: Added `UITheme::hitTestListItem()` and `UITheme::hitTestButtonMenu()` to centralize row/tile hit testing with current theme metrics.
- 2026-08-10: Enabled content tap activation in Home, file browser, recent books, Settings, language selection, external font selection, button remapping, network mode selection, WiFi network selection, Calibre settings, KOReader settings, status bar settings, OPDS server/settings screens, SD firmware selection, reader menu, EPUB/XTC chapter selection, and EPUB footnotes.
- 2026-08-10: `pio run` completed successfully. `clang-format` was not available on this host, so no automatic formatting pass was applied.

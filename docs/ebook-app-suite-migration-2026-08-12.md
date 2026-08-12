# Ebook App Suite Migration

Date: 2026-08-12
Branch: `codex/ebook-app-suite-2026-08-12`

## Source Analysis

Reference project: `/Users/tangyiyong/work/workshop/NexReader/Ebook`

- `main/app/ebook/apps/app_registry.cc` registers built-in apps as singleton `App` objects: reader, notepad, gallery, drawing, music, weather, clock, calendar, wooden fish, files, update, and settings.
- `main/app/ebook/shell/app_grid_page.cc` renders a scrollable app grid and routes taps to an app host.
- `main/app/ebook/shell/app_host_page.cc` delegates drawing/input to the active app.
- `main/app/ebook/ui/list_view.cc` provides reusable tap and swipe handling for list rows.
- `main/app/ebook/input/input_dispatch.cc` converts physical buttons into semantic actions such as Back, Home, Menu, PageNext, PagePrev, brightness, and volume.
- `main/CMakeLists.txt` shows the project depends on its own ESP-IDF BSP, FT6336U touch, GDEY027T91 panel, ES8311 audio codec, QMI8658A IMU, network services, and OTA stack.

## Hardware Notes

Netlist: `/Users/tangyiyong/work/workshop/NexReader/Netlist_PCB1_1_2026-08-12.enet`

- The reference board uses ESP32-S3-WROOM-1-N16R8.
- Audio is built around ES8311 on I2C plus I2S signals, with microphone nets `MIC1P`, `MIC1N`, and `MICBIAS12`.
- The netlist contains SDIO signals `SD_D0..SD_D3`, `SD_CMD`, and `SD_CLK`; the current PaperS3 port uses SPI SD through `HalStorage`.
- M5Stack PaperS3 already has display, GT911 touch, SD, PMIC, WiFi, USB, and BM8563 RTC support in the current HAL.
- Audio, microphone, dedicated haptics, and any external RTC should be optional compile-time features, because the current PaperS3 board does not expose the same ES8311 circuit.

## Feasibility

Feasible as a staged migration. The current project should not import the reference Router/Canvas/BSP wholesale because CrossPoint already has a stable `ActivityManager`, `UITheme`, `MappedInputManager`, `HalStorage`, and reader stack. The safe route is:

- Add an app-suite launcher Activity that mirrors the reference app grid/list model.
- Reuse existing activities for reader, file manager, gallery-through-file-browser, file transfer, OTA, and settings.
- Add low-risk pure UI apps first: clock/calendar and wooden fish.
- Keep hardware-dependent apps behind compile-time placeholders until external circuits are available.
- Port individual app logic only when it improves the reading device use case and fits flash/RAM limits.

## Requirements

- Home screen must expose a built-in apps entry.
- The apps screen must support direct tapping and swipe paging.
- Reader, recent books, files, file transfer, settings, and OTA must open existing functional activities.
- Clock/calendar must work without new hardware by using existing BM8563/NTP time facilities when available.
- Wooden fish must run without audio hardware and reserve an optional audio hook.
- Music, recorder/microphone, weather, drawing, notes, and advanced gallery features must show hardware/service status or a clear placeholder until their backing modules are implemented.
- All user-facing text must go through i18n keys.
- No new large hot-path allocations.

## Task List

- [x] Push the current stable PaperS3 touch/gesture branch.
- [x] Create the app-suite migration branch.
- [x] Analyze the reference app registry, shell, input, UI list, app modules, and hardware netlist.
- [x] Write feasibility notes and requirements.
- [x] Add app-suite launcher Activity with tap and swipe list navigation.
- [x] Add Home and ActivityManager entry points for the app suite.
- [x] Add clock/calendar Activity using existing clock facilities.
- [x] Add wooden fish Activity with no-audio fallback and optional audio compile hook.
- [x] Add placeholder Activity for hardware/service-gated apps.
- [x] Add i18n strings for English and Simplified Chinese.
- [x] Build and verify.
- [x] Commit and push the app-suite branch.
- [x] Convert app suite from list rows to tappable app tiles.
- [x] Refine app suite to a compact 3x5 icon grid.
- [x] Add file-browser edge navigation semantics for Back/Enter and top/bottom edge actions.
- [x] Add reader quick-bar Jump entry and percent slider access.
- [x] Replace the clock/date placeholder with a tappable monthly calendar view.
- [x] Add calendar API settings, monthly cache sync, and optional boot/wake WiFi auto-connect.
- [x] Add SHWGIJ lunar/almanac day API adapter with per-day sync and offline month cache merge.
- [x] Add SHWGIJ API cache-first access and request throttling for free-tier limits.
- [x] Make SHWGIJ the built-in calendar API and expose only the API token in settings.
- [x] Redraw header WiFi status as a larger high-contrast icon and keep battery percentage hidden by default.

## Implementation Log

- 2026-08-12: Pushed `codex/papers3-edge-gestures-2026-08-12` to GitHub after `pio run` succeeded.
- 2026-08-12: Created branch `codex/ebook-app-suite-2026-08-12`.
- 2026-08-12: Confirmed the reference project has a singleton app registry and app-host model, but depends on a different BSP and display/touch/audio stack.
- 2026-08-12: Chose an Activity-based migration strategy to keep the existing reader and PaperS3 HAL stable.
- 2026-08-12: Added `AppSuiteActivity` as a scrollable app launcher using existing touch tap/swipe helpers.
- 2026-08-12: Connected Home and `ActivityManager` to the new app suite entry.
- 2026-08-12: Added clock/calendar and no-audio wooden fish activities.
- 2026-08-12: Added placeholders for notepad, drawing, music, and weather, with music marked as hardware-gated.
- 2026-08-12: `git diff --check` passed.
- 2026-08-12: `pio run` completed successfully. Flash usage is now about 97.8%, so future full app ports need feature flags or partition/binary-size work.
- 2026-08-12: Converted `AppSuiteActivity` to a 2x3 tile grid with direct tap activation and swipe paging.
- 2026-08-12: Fixed file-browser left-edge swipe to go up one folder level, right-edge swipe to enter the selected folder, top-edge pull-down to show Back/Home/Settings, and bottom-edge pull-up to open Settings.
- 2026-08-12: Changed generic edge Back semantics so top-edge pull-down reveals controls instead of acting as Back.
- 2026-08-12: Added a Jump tile to the reader top quick bar. EPUB uses the existing percent slider, TXT maps the percent to lazily indexed pages, and XTC uses chapter selection.
- 2026-08-12: Added tap-to-position handling on the percent slider so touch users can jump faster than repeated button steps.
- 2026-08-12: `git diff --check` passed.
- 2026-08-12: `pio run` completed successfully after the app tile and gesture refinements.
- 2026-08-12: Refined `AppSuiteActivity` from 2x3 large tiles to a compact 2x4 icon grid using drawn line icons instead of bitmap assets to avoid extra Flash/PSRAM pressure.
- 2026-08-12: Tightened `AppSuiteActivity` further to a 3x5 icon grid, reduced icon size, and added UTF-8-safe title/status truncation for narrow tiles.
- 2026-08-12: `git diff --check` passed.
- 2026-08-12: `pio run` completed successfully after the compact icon grid update.
- 2026-08-12: Expanded `ClockCalendarActivity` into a month calendar with Monday-first weekday headers, today highlight,
  month navigation by swipe/buttons, and tap-to-open day detail. Lunar calendar, solar terms, festivals, and almanac
  metadata are left as explicit placeholders because they need a verified algorithm/data source and careful Flash sizing.
- 2026-08-12: Added `calendarApiUrl` and `autoConnectWifiOnBoot` settings. The shared settings API exposes both keys for
  a companion mobile app, while the device settings screen can edit the URL through the existing keyboard activity.
- 2026-08-12: Added `CalendarDataClient` to fetch monthly JSON, cache it under `/.crosspoint/calendar/YYYY-MM.json`, and
  display cached lunar/festival/solar-term/almanac fields in the date detail popup. Supported JSON forms are
  `days: [{date,lunar,festival,solarTerm,good,bad}]`, `data: [...]`, or date-keyed `days`/`data` objects.
- 2026-08-12: Added optional boot/wake WiFi auto-connect using the saved last-connected SSID. On success it syncs NTP and
  attempts to refresh the current month calendar cache; on failure it times out and continues normal boot.
- 2026-08-12: Added an adapter for `api.shwgij.com/api/lunars/lunar`. When `calendarApiUrl` points to that endpoint,
  the firmware appends or fills `date=YYYYMMDD000000`, accepts normal success code `201`, maps `data.Lunar`,
  `Festivals`/`OtherFestivals`, `JieQi1`/`SanFu`/`ShuJiu`, `YiDay`, and `JiDay` into the existing day detail model,
  and merges each fetched day into `/.crosspoint/calendar/YYYY-MM.json` for offline reuse.
- 2026-08-12: Changed day-detail opening so tapping a date first attempts a saved-WiFi reconnect if needed, refreshes that
  exact day from the configured API, then shows cached details. This avoids bulk-fetching a whole month from single-day
  free APIs while still preserving offline display after the first successful read.
- 2026-08-12: Added cache-first access for day sync and a 1500 ms minimum interval between SHWGIJ HTTP requests. This keeps
  repeated date taps well below the free-tier limit of 10 requests per second and avoids consuming daily quota for dates
  that are already cached on SD.
- 2026-08-12: Hardcoded the default calendar provider endpoint to `https://api.shwgij.com/api/lunars/lunar`, added
  `calendarApiToken` as the user-facing setting, and seeded it with the free API token so a fresh firmware image can fetch
  lunar data without requiring a full URL to be configured first. `calendarApiUrl` remains available as an internal
  override for custom providers.
- 2026-08-12: Replaced the scaled WiFi bitmap in Base/Lyra headers with direct 20x18 two-pixel arc drawing plus a clear
  diagonal disconnected mark. Confirmed `hideBatteryPercentage` remains defaulted to `HIDE_ALWAYS`.

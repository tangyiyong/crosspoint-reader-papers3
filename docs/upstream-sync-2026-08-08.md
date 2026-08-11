# Upstream Sync Log - 2026-08-08

## Scope

Target project:

- Path: `/Users/tangyiyong/work/workshop/NexReader/crosspoint-reader-papers3`
- Working branch: `codex/upstream-sync-2026-08-08`
- Backup branch before this pass: `backup/pre-upstream-sync-2026-08-08`
- Host platform: `Darwin`

Source repositories:

- `/Users/tangyiyong/work/workshop/NexReader/crosspoint-reader`
  - GitHub: `https://github.com/crosspoint-reader/crosspoint-reader.git`
  - Remote branch: `origin/develop`
  - Latest fetched commit: `e00f5958` (`v1.5.0`)
- `/Users/tangyiyong/work/workshop/NexReader/crosspoint-reader-papers3-origin`
  - GitHub: `https://github.com/juicecultus/crosspoint-reader-papers3.git`
  - Remote branch: `origin/master`
  - Latest fetched commit: `d9792a5` (`1.3.2`)

## Fetch Result

`crosspoint-reader` advanced beyond the previous sync baseline around `fecb9eb3` with these default-branch commits:

| Commit | Summary | Decision |
|---|---|---|
| `046827fc` | Fix EPUB ruby overflow and unwanted short CJK lines | Deferred. Current PaperS3 target does not include the upstream ruby/SUP/SUB text model, so this is not a direct patch. |
| `28f3260c` | Update translations | Partially deferred. Current target carries PaperS3/CJK-specific translation keys; wholesale replacement would drop local strings. |
| `5eec70de` | Add IPA intervals to SD fonts | Deferred. Current target does not include upstream `fontconvert_sdcard.py` / `sd-fonts.yaml` pipeline. Built-in CJK conversion already includes `0x02C0..0x02FF`. |
| `c507e544` | Fix thread-safety issues in credential store | Ported, adapted to the target's custom `WifiCredentialStore`. |
| `255bab31` | Read `platformio.ini` as UTF-8 in `git_branch.py` | Ported. |
| `e00f5958` | Guard against cross-chip firmware installs | Partially ported. SD firmware install now surfaces a wrong-device error; OTA path needs a larger downloader rewrite to pre-read chip_id before flash. |

`crosspoint-reader-papers3-origin` has no commits newer than `d9792a5` on `origin/master`, so there is no new PaperS3-origin code to port in this pass.

## Implemented

### WiFi Credential Store Thread Safety

Evidence in target before the change:

- `src/WifiCredentialStore.h` stored `credentials` and `lastConnectedSsid` directly in the singleton.
- `src/WifiCredentialStore.cpp` mutated and returned pointers/references to those fields without locking.
- `src/activities/network/WifiSelectionActivity.cpp` held pointers returned by `findCredential()`.

Changes:

- Added a `std::mutex` to guard in-memory WiFi credential strings.
- Changed `findCredential()` to return `std::optional<WifiCredential>` by value instead of a pointer into the internal vector.
- Added bounded snapshot APIs for JSON save and UI/API consumers.
- Updated WiFi selection code to use the copied credential.
- Reworked JSON load to build a local vector, reserve up to `MAX_NETWORKS`, then atomically replace the store contents.

Resource note:

- The new snapshot vectors are bounded by `MAX_NETWORKS == 8`, so the temporary heap use is small and short-lived.
- SD writes happen after the credential mutex is released, avoiding a long lock during filesystem I/O.

### Build Version UTF-8 Fix

Evidence in target before the change:

- `platformio.ini` contains non-ASCII comments.
- `scripts/git_branch.py` used `config.read(ini_path)` without an explicit encoding.

Change:

- `scripts/git_branch.py` now reads `platformio.ini` with `encoding='utf-8'`, matching upstream `255bab31`.

### SD Firmware Wrong-Device Error

Evidence in target before the change:

- `src/network/FirmwareInstaller.cpp` already checked `esp_image_header_t::chip_id` against `ESP_CHIP_ID_ESP32S3`.
- That path returned the generic `INVALID_IMAGE`, so the UI could not explain the actual cause.

Changes:

- Added `FirmwareInstaller::Error::WRONG_DEVICE`.
- `validateImageHeader()` now returns `WRONG_DEVICE` for a non-ESP32-S3 image.
- `SdFirmwareUpdateActivity` maps that error to `STR_SD_FIRMWARE_ERR_WRONG_DEVICE`.
- Added English, Chinese Simplified, Japanese, and French source translations. Other languages fall back to English through the existing generator.

Resource note:

- No new heap allocation was added for this path. The header validation still reads one `esp_image_header_t` stack object and rewinds the file.

## Deferred Work

### EPUB Ruby / CJK Line-Break Fix

Upstream `046827fc` depends on data structures and style flags that are not present in the current PaperS3 target:

- No `RUBY`, `RUBY_CONTINUE`, `SUP`, or `SUB` style support was found in the target `lib/Epub` / `lib/EpdFont` code.
- Porting only the line-breaking part would not compile or would be inert without the wider ruby parser/rendering feature.

Recommended next step:

- Treat ruby/SUP/SUB support as a separate feature migration from upstream/CJK, including cache format versioning and rendering tests.

### SD Font IPA Generation

Upstream `5eec70de` modifies the SD-card font generator. The current PaperS3 target does not have:

- `docs/sd-card-fonts.md`
- `lib/EpdFont/scripts/fontconvert_sdcard.py`
- `lib/EpdFont/scripts/sd-fonts.yaml`

The target's built-in CJK conversion path already passes `--additional-intervals 0x02C0,0x02FF`, so built-in font coverage for IPA-adjacent spacing modifier letters is already partially covered.

Recommended next step:

- Port the upstream SD font generator only if the external font system is expanded to support generated `.cpfont` packages directly.

### OTA Wrong-Device Preflight

Upstream `e00f5958` reads the first firmware bytes during a custom download/write loop and aborts before flashing a wrong-MCU image. The current PaperS3 OTA path uses `esp_https_ota`, whose public `esp_https_ota_get_img_desc()` API exposes `esp_app_desc_t`, not the `esp_image_header_t::chip_id` field.

Recommended next step:

- Rework OTA to download to SD and reuse `FirmwareInstaller`, or replace `esp_https_ota` with a streamed `esp_ota_write` path that buffers the first 14 bytes and checks `chip_id` before writing.

## Verification

- `python scripts/gen_i18n.py lib/I18n/translations lib/I18n/` succeeded.
  - Existing fallback warnings remain, including `SV` carrying `STR_MENU_FILE_BROWSER` not present in English.
- `pio run` succeeded after this migration batch.
  - RAM: 60,728 / 327,680 bytes (18.5%).
  - Flash: 7,598,243 / 7,864,320 bytes (96.6%).
  - Total image size: 7,598,591 bytes.

## Local Notes

- `books/` remains an untracked local SD-card test directory and was intentionally not included.
- `src/idf_component.yml` was generated by the PlatformIO framework install/build and removed from the worktree.

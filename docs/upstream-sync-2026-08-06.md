# Upstream Sync Log - 2026-08-06

## Scope

Compare and selectively port new work from:

- `/Users/tangyiyong/work/workshop/NexReader/crosspoint-reader`
  - GitHub: https://github.com/crosspoint-reader/crosspoint-reader.git
  - Local branch: `develop`
  - Remote HEAD: `origin/develop`
- `/Users/tangyiyong/work/workshop/NexReader/crosspoint-reader-cjk`
  - GitHub: https://github.com/aBER0724/crosspoint-reader-cjk.git
  - Local branch: `master`
  - Remote HEAD: `origin/master`

Current target project:

- Path: `/Users/tangyiyong/work/workshop/NexReader/crosspoint-reader-papers3`
- Branch: `codex/upstream-sync-2026-08-06`
- Backup branch created before this sync: `backup/2026-08-06-before-upstream-sync`
- Host platform: `Darwin`

## Current Target Worktree Note

The target project already had in-progress migration changes before this upstream-sync pass, including:

- Line spacing percentage selector.
- External reader font preview.
- SD-card firmware updater.
- I18n additions for the above.
- A partial font ID generator fix started before this sync log.

These changes are preserved and treated as existing local work.

## Upstream Fetch Result

Both source repositories were fetched on 2026-08-06.

- `crosspoint-reader`: local `HEAD` and `origin/HEAD` are both `fecb9eb3`; default branch has no local-vs-remote delta.
- `crosspoint-reader-cjk`: local `HEAD` and `origin/HEAD` are both `db8a5c4`; default branch has no local-vs-remote delta.

New work to evaluate is concentrated in active upstream feature/release branches rather than the default branches.

After the first sync pass, `crosspoint-reader` received a new commit on `origin/feat-touch-ui`:

- `0809d0b6` - Fix buffer overflows and dictionary word trimming.
  - PaperS3-applicable parts: WDT reset classification, longer translated UI buffers, WiFi scan dedupe.
  - Not applicable: `src/util/Dictionary.cpp` is not present in the current PaperS3 target.

## Candidate Branches

### crosspoint-reader

- `origin/feat-touch-ui`
  - Latest: `0809d0b6` - Fix buffer overflows and dictionary word trimming.
  - Relevant themes: touch UI polish, frontlight panel, render tap filtering, X4 Pro HAL work.
  - Risk: medium/high for PaperS3 because it touches HAL, input mapping, UI layout, and non-PaperS3 boards.
- `origin/feat-chunk-bitmaps`
  - Latest: `27c4be8a` - Implement chunked allocation for glyph bitmaps.
  - Relevant themes: reduce heap fragmentation for SD-card font glyph bitmap allocations.
  - Risk: medium. Current PaperS3 project uses `lib/ExternalFont`, not upstream `SdCardFont`, so the idea is portable but not a direct patch.
- `origin/feat-bluetooth`
  - Latest: `28af4189` - Chunk font bitmap allocations to avoid fragmentation.
  - Relevant themes: BLE page turn, heap diagnostics, idle glyph prewarm, async display refresh, lower TLS memory.
  - Risk: high. BLE stack and async display increase flash and RAM pressure; target firmware is already near the app partition limit.
- `origin/feat-sd-web-plugins`
  - Latest: `506ce26d` - Fix line formatting in LOG_ERR call.
  - Relevant themes: chunked resumable downloads, watchdog handling, streamed decryption, web plugin pages.
  - Risk: medium/high. Some web transfer fixes are useful; plugin host is likely out of scope for a dedicated reader unless explicitly needed.
- `origin/fix-wifi-cred-corruption`
  - Latest: `1498ab79` - Add length validation for obfuscated settings values.
  - Relevant themes: CRC/integrity validation and safer credential deserialization.
  - Risk: low/medium and high value for stability.
- `origin/feat-dark-mode`
  - Latest: `ffaaf041` - Add display inversion with image polarity preservation.
  - Current target status: already mostly present in `GfxRenderer` and settings.
  - Risk: low for review only; no broad port needed now.
- `origin/chore/update-translations`
  - Latest: `63034e19` - Hebrew strings.
  - Relevant themes: language coverage updates.
  - Risk: low, but needs I18n generator review and flash-size check.

### crosspoint-reader-cjk

- `origin/sync/upstream-1.1.1-patch`
  - Latest: `99be8d4` - Selectively merge upstream changes into CJK fork.
  - Relevant themes include first-line indent fixes, SD font generation, Vietnamese/Slovak/Hebrew locales, Polish/Swedish hyphenation, RTL support, bookmarks, sup/sub support, streamed grayscale/image memory reductions, asset path decoding, and footnote fixes.
  - Current target status: some pieces are already present (`firstLineIndent`, footnotes, streaming EPUB temp files, dark mode, CJK rendering, external font basics).
  - Risk: mixed; must cherry-pick small fixes rather than merge branch wholesale.
- `origin/release/sd-recovery-package`
  - Latest: `c65d88a` - Document SD recovery flow.
  - Current target status: target now has an SD firmware updater; docs can be adapted later.
  - Risk: low.

## First Migration Plan

1. Stabilize font identity and external font dispatch in the target.
   - Rationale: target currently has duplicate generated font IDs for CJK reader/UI fonts, and `GfxRenderer` treats any `fontId <= -1000` as external reader font. That can route built-in negative IDs through the external-font path and can make different reader sizes share EPUB section caches.
   - Status: implemented; pending build verification.
2. Port low-risk credential integrity checks from `fix-wifi-cred-corruption`.
   - Rationale: protects WiFi/OPDS/KOReader JSON settings from corrupted base64 or impossible plaintext lengths.
   - Status: implemented; pending build verification.
3. Evaluate chunked glyph cache allocation against target `ExternalFont`.
   - Rationale: upstream changed `SdCardFont`, while target uses a different external-font subsystem. Port mechanism only if it reduces contiguous PSRAM pressure without complicating glyph cache lookup.
   - Status: pending.
4. Review touch UI branch for PaperS3-specific reader input fixes only.
   - Rationale: target is touch-only, but X4/frontlight/general HAL changes should not be merged wholesale.
   - Status: pending.
5. Review CJK sync branch for small EPUB rendering fixes not already present.
   - Rationale: likely candidates are percent-decoded asset/footnote paths, span-anchor skipping, underline/sup/sub fixes, and hyphenation language additions.
   - Status: partially implemented. Percent-decoded paths, footnote target decode, span-anchor heap guard, TOC anchor preservation, and image-page ghost cleanup are ported. SUP/SUB underline scaling is not applicable yet because this target does not have SUP/SUB font style bits.

## Out-of-Scope Until Explicitly Requested

- Full BLE page-turn support from `feat-bluetooth`, due flash/RAM pressure and new radio lifecycle interactions.
- Full upstream web plugin host, unless the product direction expands beyond the dedicated reader workflow.
- Full upstream/X4 HAL or frontlight support; PaperS3 hardware does not share those peripherals.

## Implementation Notes

### Font identity and external-font dispatch

- Updated `lib/EpdFont/scripts/build-font-ids.sh` so CJK reader/UI roles that share the same source font data can still receive distinct generated IDs.
- Regenerated `src/fontIds.h` values for `NOTOSANS_16_FONT_ID`, `NOTOSANS_18_FONT_ID`, `UI_10_FONT_ID`, `UI_12_FONT_ID`, and `SMALL_FONT_ID`.
- Tightened `GfxRenderer::isExternalReaderFontId()` so it only matches `FontManager::getSelectedFontId()` when an external reader font is actually loaded.
- No new heap allocation was added.

### Credential integrity and bounded decode

- Added `lib/Serialization/CredentialIntegrity.h` with constexpr IEEE CRC-32 helpers.
- Added a bounded `obfuscation::deobfuscateFromBase64()` overload that rejects decoded data larger than the caller's maximum before allocating the decoded string.
- `JsonSettingsIO` now bounds obfuscated decode for CrossPoint settings, legacy OPDS settings, KOReader settings, OPDS server passwords, and WiFi credentials.
- WiFi credentials now save `password_len` and `password_crc32`; existing JSON without these fields remains accepted and will resave after a successful load.
- No large heap allocation was added; the bounded decode explicitly prevents oversized allocation from corrupted base64 data.

### EPUB internal path decoding and anchor guards

- Ported the small shared EPUB-internal URI escape decoder from CJK/upstream work into `lib/FsHelpers`.
- Applied decoding to OPF manifest/guide hrefs, guide cover image fallback, TOC NAV/NCX paths and anchors, inline HTML image paths, and footnote/spine target resolution.
- Incremented `book.bin` cache version from 5 to 6 and documented the invalidation in `docs/file-formats.md`.
- Added a chapter anchor safety guard based on upstream `skip <span> anchors` work:
  - Non-navigable `<span id="...">` anchors are skipped by default.
  - Recorded anchors are capped at 1024 per chapter.
  - TOC anchors for the current spine are collected and passed to the parser so genuine TOC page boundaries bypass the skip/cap behavior.
- The TOC anchor vector is temporary during section build. It is counted first and `reserve()` is called before the push loop to avoid repeated vector growth.

### Image-page ghost cleanup

- Ported the applicable half-refresh scheduling portion of the upstream/CJK grayscale image ghosting fix.
- Current PaperS3 uses `GRAYSCALE_DIRECT`, not upstream tiled grayscale strips, so the strip-intersection optimization from `ImageBlock` is not directly applicable.
- After an anti-aliased image page uses the double fast-refresh path, `pagesUntilFullRefresh` is set to 1 so the next ordinary text page uses the half-refresh cleanup path.

### Not Applicable in This Pass

- `fad1a80` SUP/SUB underline scaling was evaluated and skipped because this target's `EpdFontFamily::Style` currently only defines regular/bold/italic/underline bits. Porting that fix requires the broader SUP/SUB style/rendering feature first.
- `03f73fa` avoid ZIP-wide CSS scanning was evaluated. The current PaperS3 target already lacks the ZIP-wide CSS discovery path and only consumes CSS hrefs collected from `content.opf`, so there is no direct patch to apply.
- `a60f31c` invalid web font upload filename handling was evaluated. The current PaperS3 web server does not have the separate font upload handler touched by that commit; external font scanning already validates filenames through `FontFilenameParser`.

### Font prewarm underline scan

- Ported `db94a86` in the target's current renderer architecture.
- Added `GfxRenderer::isFontCacheScanning()` as a small pass-through to `FontCacheManager::isScanning()`.
- `TextBlock::render()` now skips underline width/advance measurement while the font prewarm scan pass is active.
- No heap allocation was added. The mechanism avoids SD glyph lookup work during scan mode because text capture still happens in `drawText()`, while underline drawing is deferred to the real render pass.

### KOReader chapter-start sync

- Ported the chapter-start portion of `bb078ba`.
- `ProgressMapper::toCrossPoint()` now recognizes KOReader XPath forms that point to the start of a `DocFragment`/chapter and resolves intra-spine progress to `0.0` instead of falling back to byte-ratio estimation.
- No heap allocation was added; the helper performs bounded string checks on the existing XPath string.

### Reader touch/page-turn guards

- Ported the PaperS3-applicable behavior from upstream `faefd59b`.
- Manual page turns are ignored while `RenderLock::peek()` reports an active render, or within a 200 ms gap after the last manual turn. This avoids accepting a second tap before a slow anti-aliased/image render has committed its display baseline.
- Ported the current-architecture portion of CJK `f055fdd`: when long-press chapter skip is enabled, long-pressing previous while not on the first page now jumps to the start of the current chapter before attempting to move to the previous chapter.
- No heap allocation was added.

### UI buffer bounds and WDT panic retention

- Ported the PaperS3-applicable parts of upstream `0809d0b6`.
- Enlarged the translated `STR_NETWORKS_FOUND` count buffer from 32 to 64 bytes.
- Enlarged clock sync/current-time translated line buffers from 24/32 to 64 bytes.
- Added ESP32 WDT reset reasons (`ESP_RST_INT_WDT`, `ESP_RST_TASK_WDT`, `ESP_RST_WDT`) to the panic-reboot detection path so crash logs are preserved after watchdog resets.
- No heap allocation was added; these are small stack buffer size changes.

### WiFi scan dedupe without std::map

- Ported CJK/upstream `3a1e9f3` into the current WiFi selection activity.
- Removed per-node `std::map` allocations from scan dedupe.
- Reused the existing `networks` vector, calls `reserve(scanResult)` before the push loop, and keeps the strongest RSSI per SSID in place.
- Removed the unused `WifiNetworkInfo::ipAddress` member.
- Allocation note: the vector already existed in the activity. The new `reserve()` pre-allocates once for the scan result count and avoids repeated vector growth and `std::map` node allocations.

### Streaming image pixel cache

- Ported the core memory-saving behavior from CJK/upstream `d9bcef7`, adapted to the current PaperS3 renderer which does not have upstream `DirectPixelWriter`.
- Replaced full-image `.pxc` write buffers (previously up to 256 KB) with a streaming band buffer capped at 24 KB plus one zero row.
- The band buffer is allocated with `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` on PaperS3, so JPEG/PNG decoder DRAM pressure is reduced.
- JPEG cache streaming advances by decoded block top row; PNG cache streaming advances by output row. A write failure disables the current cache and deletes the partial file.
- Cached image reads now batch several rows per SD read with an approximately 4 KB SPIRAM read buffer, falling back to one row if needed.
- Image blocks now skip work during the font prewarm scan pass because images do not contribute glyphs.

### Additional not-applicable or already-covered checks

- `f2e3d11` hanging-indent overlap was evaluated. The current PaperS3 `ParsedText` path already preserves negative word x positions instead of clamping them to zero, so the problematic behavior is already absent.
- `0809d0b6` dictionary curly-quote trimming was evaluated, but this target does not contain `src/util/Dictionary.cpp`.
- Full CJK/upstream RTL, SUP/SUB, bookmarks, additional bundled fonts, and new locale/font-coverage expansions remain deferred because they are larger feature migrations and the current app partition is already about 96.5% used.

## Verification

- `pio run` succeeded after the first migration batch.
  - RAM: 60,720 / 327,680 bytes (18.5%).
  - Flash: 7,586,283 / 7,864,320 bytes (96.5%).
- `pio run` succeeded after the EPUB path/anchor/image cleanup batch.
  - RAM: 60,720 / 327,680 bytes (18.5%).
  - Flash: 7,589,059 / 7,864,320 bytes (96.5%).
  - Total image size: 7,589,407 bytes.
- `pio run` succeeded after the font-scan and KOReader sync batch.
  - RAM: 60,720 / 327,680 bytes (18.5%).
  - Flash: 7,589,755 / 7,864,320 bytes (96.5%).
  - Total image size: 7,590,103 bytes.
- `pio run` succeeded after the reader touch/page-turn guard batch.
  - RAM: 60,720 / 327,680 bytes (18.5%).
  - Flash: 7,589,859 / 7,864,320 bytes (96.5%).
  - Total image size: 7,590,207 bytes.
- `pio run` succeeded after the UI buffer/WDT and WiFi scan dedupe batch.
  - RAM: 60,720 / 327,680 bytes (18.5%).
  - Flash: 7,584,267 / 7,864,320 bytes (96.4%).
  - Total image size: 7,584,615 bytes.
- `pio run` succeeded after the streaming image cache batch.
  - RAM: 60,720 / 327,680 bytes (18.5%).
  - Flash: 7,586,199 / 7,864,320 bytes (96.5%).
  - Total image size: 7,586,547 bytes.

#pragma once

#include <cstddef>

namespace ExternalFontCachePolicy {

// Keep the glyph cache modest even though PaperS3 has PSRAM: rendering still
// shares memory with EPUB/TXT layout caches, image decode buffers, and the
// framebuffer. The backing allocation is routed to PSRAM in ExternalFont.cpp.
constexpr size_t kGlyphCacheSize = 128;
constexpr size_t kPreloadLimit = 128;

static_assert(kPreloadLimit <= kGlyphCacheSize, "preload limit must fit in glyph cache");

}  // namespace ExternalFontCachePolicy

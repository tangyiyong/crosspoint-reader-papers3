#include "TxtEncoding.h"

#include <Utf8.h>

#include "GbkUnicodeTable.generated.h"

namespace {

constexpr uint32_t UTF8_REPLACEMENT = REPLACEMENT_GLYPH;

bool isUtf8Continuation(const uint8_t b) { return (b & 0xC0) == 0x80; }

int utf8LenFromLead(const uint8_t lead) {
  if (lead < 0x80) return 1;
  if ((lead >> 5) == 0x6) return 2;
  if ((lead >> 4) == 0xE) return 3;
  if ((lead >> 3) == 0x1E) return 4;
  return 0;
}

bool isValidUtf8Sample(const uint8_t* data, const size_t len) {
  size_t i = 0;
  while (i < len) {
    const uint8_t lead = data[i];
    const int bytes = utf8LenFromLead(lead);
    if (bytes == 0) return false;
    if (bytes == 1) {
      i++;
      continue;
    }
    if (i + bytes > len) {
      return true;
    }
    for (int j = 1; j < bytes; j++) {
      if (!isUtf8Continuation(data[i + j])) return false;
    }

    uint32_t cp = lead & ((1 << (7 - bytes)) - 1);
    for (int j = 1; j < bytes; j++) {
      cp = (cp << 6) | (data[i + j] & 0x3F);
    }
    const bool overlong = (bytes == 2 && cp < 0x80) || (bytes == 3 && cp < 0x800) || (bytes == 4 && cp < 0x10000);
    const bool surrogate = (cp >= 0xD800 && cp <= 0xDFFF);
    if (overlong || surrogate || cp > 0x10FFFF) return false;
    i += bytes;
  }
  return true;
}

uint16_t gbkToUnicode(const uint16_t gbk) {
  size_t left = 0;
  size_t right = GBK_UNICODE_TABLE_SIZE;
  while (left < right) {
    const size_t mid = left + (right - left) / 2;
    const uint16_t key = GBK_UNICODE_TABLE[mid].gbk;
    if (key == gbk) return GBK_UNICODE_TABLE[mid].unicode;
    if (key < gbk) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }
  return UTF8_REPLACEMENT;
}

void appendUtf8(std::string& out, const uint32_t cp) {
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

size_t nextUtf8SourceLen(const uint8_t* data, const size_t len) {
  if (len == 0) return 0;
  const int bytes = utf8LenFromLead(data[0]);
  if (bytes <= 1 || static_cast<size_t>(bytes) > len) return 1;
  for (int i = 1; i < bytes; i++) {
    if (!isUtf8Continuation(data[i])) return 1;
  }
  return static_cast<size_t>(bytes);
}

size_t nextGbkSourceLen(const uint8_t* data, const size_t len) {
  if (len == 0) return 0;
  if (data[0] < 0x80) return 1;
  if (len >= 2 && data[0] >= 0x81 && data[0] <= 0xFE && data[1] >= 0x40 && data[1] <= 0xFE && data[1] != 0x7F) {
    return 2;
  }
  return 1;
}

uint32_t decodeGbkChar(const uint8_t* data, const size_t len, size_t& consumed) {
  consumed = nextGbkSourceLen(data, len);
  if (consumed == 0) return 0;
  if (data[0] < 0x80) return data[0];
  if (consumed == 2) {
    return gbkToUnicode(static_cast<uint16_t>((data[0] << 8) | data[1]));
  }
  return UTF8_REPLACEMENT;
}

size_t utf8EncodedLenFromSource(const uint8_t* data, const size_t len, const TxtEncoding encoding, size_t& consumed) {
  if (encoding == TxtEncoding::Gbk) {
    const uint32_t cp = decodeGbkChar(data, len, consumed);
    if (cp <= 0x7F) return 1;
    if (cp <= 0x7FF) return 2;
    if (cp <= 0xFFFF) return 3;
    return 4;
  }

  consumed = nextUtf8SourceLen(data, len);
  return consumed;
}

}  // namespace

TxtEncoding detectTxtEncoding(const uint8_t* data, const size_t len) {
  if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
    return TxtEncoding::Utf8Bom;
  }
  return isValidUtf8Sample(data, len) ? TxtEncoding::Utf8 : TxtEncoding::Gbk;
}

std::string txtBytesToUtf8(const uint8_t* data, size_t len, const TxtEncoding encoding) {
  if (encoding == TxtEncoding::Utf8Bom) {
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
      data += 3;
      len -= 3;
    }
    return std::string(reinterpret_cast<const char*>(data), len);
  }

  if (encoding == TxtEncoding::Utf8) {
    return std::string(reinterpret_cast<const char*>(data), len);
  }

  std::string out;
  out.reserve(len * 3);
  size_t pos = 0;
  while (pos < len) {
    size_t consumed = 0;
    const uint32_t cp = decodeGbkChar(data + pos, len - pos, consumed);
    if (consumed == 0) break;
    appendUtf8(out, cp);
    pos += consumed;
  }
  return out;
}

size_t txtSourceBytesForUtf8Prefix(const uint8_t* data, const size_t len, const TxtEncoding encoding,
                                   const size_t utf8PrefixBytes) {
  size_t pos = 0;
  size_t produced = 0;
  while (pos < len && produced < utf8PrefixBytes) {
    size_t consumed = 0;
    const size_t outLen = utf8EncodedLenFromSource(data + pos, len - pos, encoding, consumed);
    if (consumed == 0 || produced + outLen > utf8PrefixBytes) break;
    produced += outLen;
    pos += consumed;
  }
  return pos;
}

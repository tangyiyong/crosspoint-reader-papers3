#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

enum class TxtEncoding : uint8_t { Utf8, Utf8Bom, Gbk };

TxtEncoding detectTxtEncoding(const uint8_t* data, size_t len);
std::string txtBytesToUtf8(const uint8_t* data, size_t len, TxtEncoding encoding);
size_t txtSourceBytesForUtf8Prefix(const uint8_t* data, size_t len, TxtEncoding encoding, size_t utf8PrefixBytes);

#include "QuoteDataClient.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "CrossPointSettings.h"
#include "network/HttpDownloader.h"
#include "util/WifiUtils.h"

namespace {
constexpr char QUOTE_CACHE_DIR[] = "/.crosspoint/quote";
constexpr char QUOTE_CACHE_PATH[] = "/.crosspoint/quote/daily.json";
constexpr char SHWGIJ_QUOTE_API_URL[] = "https://api.shwgij.com/api/randtext/get";
constexpr uint8_t SHWGIJ_QUOTE_TYPE_MIN = 1;
constexpr uint8_t SHWGIJ_QUOTE_TYPE_COUNT = 14;
constexpr unsigned long SHWGIJ_MIN_REQUEST_INTERVAL_MS = 1500;
unsigned long lastQuoteRequestAt = 0;

void copyJsonString(char* dest, const size_t destSize, JsonVariantConst value) {
  if (destSize == 0) return;
  const char* text = value | "";
  snprintf(dest, destSize, "%s", text);
}
}  // namespace

bool QuoteDataClient::hasConfiguredApi() { return SETTINGS.calendarApiToken[0] != '\0'; }

bool QuoteDataClient::getLocalDate(char* outDate, const size_t outDateSize) {
  if (outDateSize == 0) {
    return false;
  }
  outDate[0] = '\0';

  const time_t now = time(nullptr);
  if (now <= 1700000000) {
    return false;
  }

  const int offsetQuarterHours = static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48;
  const time_t localNow = now + offsetQuarterHours * 15 * 60;
  struct tm timeinfo;
  gmtime_r(&localNow, &timeinfo);
  snprintf(outDate, outDateSize, "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  return true;
}

bool QuoteDataClient::ensureWifiConnectedFromSavedCredential() {
  return WifiUtils::ensureConnectedFromSavedCredential();
}

void QuoteDataClient::waitForApiRateLimit() {
  const unsigned long now = millis();
  if (lastQuoteRequestAt != 0 && now - lastQuoteRequestAt < SHWGIJ_MIN_REQUEST_INTERVAL_MS) {
    delay(SHWGIJ_MIN_REQUEST_INTERVAL_MS - (now - lastQuoteRequestAt));
  }
  lastQuoteRequestAt = millis();
}

std::string QuoteDataClient::buildUrl(const bool dailyMode) {
  std::string url = SHWGIJ_QUOTE_API_URL;
  const uint8_t quoteType = SHWGIJ_QUOTE_TYPE_MIN + static_cast<uint8_t>(esp_random() % SHWGIJ_QUOTE_TYPE_COUNT);
  char suffix[24];
  snprintf(suffix, sizeof(suffix), "&type=%u&m=%u", quoteType, dailyMode ? 1 : 0);
  url += "?key=";
  url += SETTINGS.calendarApiToken;
  url += suffix;
  return url;
}

bool QuoteDataClient::isCacheFreshForToday() {
  char today[11];
  if (!getLocalDate(today, sizeof(today))) {
    return false;
  }

  QuoteInfo cached;
  return loadCached(cached) && strcmp(cached.date, today) == 0;
}

bool QuoteDataClient::syncDailyIfNeeded(const bool allowSavedWifiConnect) {
  if (!hasConfiguredApi()) {
    return false;
  }
  if (isCacheFreshForToday()) {
    return true;
  }
  if (!WifiUtils::isConnected() && (!allowSavedWifiConnect || !ensureWifiConnectedFromSavedCredential())) {
    return false;
  }
  return fetchAndCache(true);
}

bool QuoteDataClient::fetchRandom(const bool allowSavedWifiConnect) {
  if (!hasConfiguredApi()) {
    return false;
  }
  if (!WifiUtils::isConnected() && (!allowSavedWifiConnect || !ensureWifiConnectedFromSavedCredential())) {
    return false;
  }
  return fetchAndCache(false);
}

bool QuoteDataClient::fetchAndCache(const bool dailyMode) {
  std::string body;
  const std::string url = buildUrl(dailyMode);
  waitForApiRateLimit();
  if (!HttpDownloader::fetchUrl(url, body)) {
    LOG_ERR("QUOTE", "Failed to fetch quote API");
    return false;
  }

  if (body.empty() || body.size() > 4096) {
    LOG_ERR("QUOTE", "Invalid quote response size: %zu", body.size());
    return false;
  }

  JsonDocument responseDoc;
  const DeserializationError responseError = deserializeJson(responseDoc, body);
  if (responseError) {
    LOG_ERR("QUOTE", "Quote API JSON parse failed: %s", responseError.c_str());
    return false;
  }

  const int code = responseDoc["code"] | 0;
  if (code != 200 && code != 201) {
    LOG_ERR("QUOTE", "Quote API returned code: %d", code);
    return false;
  }

  const JsonObjectConst data = responseDoc["data"].as<JsonObjectConst>();
  if (data.isNull()) {
    LOG_ERR("QUOTE", "Quote API response has no data object");
    return false;
  }

  char today[11];
  getLocalDate(today, sizeof(today));

  JsonDocument cacheDoc;
  cacheDoc["date"] = today;
  cacheDoc["mode"] = dailyMode ? "daily" : "random";
  cacheDoc["type"] = data["type"] | "";
  cacheDoc["text"] = data["text"] | "";
  cacheDoc["cn"] = data["cn"] | "";
  cacheDoc["apiUpdate"] = responseDoc["update"] | "";

  std::string out;
  serializeJson(cacheDoc, out);
  if (out.empty() || out.size() > 2048) {
    LOG_ERR("QUOTE", "Quote cache too large: %zu", out.size());
    return false;
  }

  Storage.mkdir("/.crosspoint");
  Storage.mkdir(QUOTE_CACHE_DIR);
  return Storage.writeFile(QUOTE_CACHE_PATH, String(out.c_str()));
}

bool QuoteDataClient::loadCached(QuoteInfo& outInfo) {
  outInfo = QuoteInfo{};
  if (!Storage.exists(QUOTE_CACHE_PATH)) {
    return false;
  }

  const String json = Storage.readFile(QUOTE_CACHE_PATH);
  if (json.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("QUOTE", "Quote cache JSON parse failed: %s", error.c_str());
    return false;
  }

  copyJsonString(outInfo.date, sizeof(outInfo.date), doc["date"]);
  copyJsonString(outInfo.type, sizeof(outInfo.type), doc["type"]);
  copyJsonString(outInfo.text, sizeof(outInfo.text), doc["text"]);
  copyJsonString(outInfo.cn, sizeof(outInfo.cn), doc["cn"]);
  outInfo.hasAny = outInfo.text[0] != '\0';
  return outInfo.hasAny;
}

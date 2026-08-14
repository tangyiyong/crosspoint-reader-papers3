#include "TodayHistoryClient.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <utility>

#include "CrossPointSettings.h"
#include "network/HttpDownloader.h"
#include "util/WifiUtils.h"

namespace {
constexpr char TODAY_CACHE_DIR[] = "/.crosspoint/today";
constexpr char TODAY_CACHE_PATH[] = "/.crosspoint/today/onthisday.json";
constexpr char SHWGIJ_TODAY_API_URL[] = "https://api.shwgij.com/api/today/onthisday";
constexpr unsigned long SHWGIJ_TODAY_MIN_REQUEST_INTERVAL_MS = 1500;
constexpr size_t MAX_TODAY_RESPONSE_BYTES = 30000;
constexpr size_t MAX_TODAY_CACHE_BYTES = 24000;
constexpr size_t MAX_TODAY_EVENTS = 16;
unsigned long lastTodayRequestAt = 0;

void copyJsonString(char* dest, const size_t destSize, JsonVariantConst value) {
  if (destSize == 0) return;
  const char* text = value | "";
  snprintf(dest, destSize, "%s", text);
}
}  // namespace

bool TodayHistoryClient::hasConfiguredApi() { return SETTINGS.calendarApiToken[0] != '\0'; }

bool TodayHistoryClient::ensureWifiConnectedFromSavedCredential() {
  return WifiUtils::ensureConnectedFromSavedCredential();
}

void TodayHistoryClient::waitForApiRateLimit() {
  const unsigned long now = millis();
  if (lastTodayRequestAt != 0 && now - lastTodayRequestAt < SHWGIJ_TODAY_MIN_REQUEST_INTERVAL_MS) {
    delay(SHWGIJ_TODAY_MIN_REQUEST_INTERVAL_MS - (now - lastTodayRequestAt));
  }
  lastTodayRequestAt = millis();
}

std::string TodayHistoryClient::buildUrl() {
  std::string url = SHWGIJ_TODAY_API_URL;
  url += "?key=";
  url += SETTINGS.calendarApiToken;
  return url;
}

bool TodayHistoryClient::syncToday(const bool allowSavedWifiConnect) {
  if (!hasConfiguredApi()) {
    return false;
  }
  if (!WifiUtils::isConnected() && (!allowSavedWifiConnect || !ensureWifiConnectedFromSavedCredential())) {
    return false;
  }

  std::string body;
  const std::string url = buildUrl();
  waitForApiRateLimit();
  if (!HttpDownloader::fetchUrl(url, body)) {
    LOG_ERR("TODAY", "Failed to fetch today history API");
    return false;
  }

  if (body.empty() || body.size() > MAX_TODAY_RESPONSE_BYTES) {
    LOG_ERR("TODAY", "Invalid today history response size: %zu", body.size());
    return false;
  }

  JsonDocument responseDoc;
  const DeserializationError responseError = deserializeJson(responseDoc, body);
  if (responseError) {
    LOG_ERR("TODAY", "Today history JSON parse failed: %s", responseError.c_str());
    return false;
  }

  const int code = responseDoc["code"] | 0;
  if (code != 200 && code != 201) {
    LOG_ERR("TODAY", "Today history API returned code: %d", code);
    return false;
  }

  if (!responseDoc["data"].is<JsonArrayConst>()) {
    LOG_ERR("TODAY", "Today history response has no data array");
    return false;
  }

  JsonDocument cacheDoc;
  cacheDoc["update"] = responseDoc["update"] | "";
  JsonArray events = cacheDoc["events"].to<JsonArray>();
  size_t count = 0;
  for (JsonObjectConst item : responseDoc["data"].as<JsonArrayConst>()) {
    if (count >= MAX_TODAY_EVENTS) {
      break;
    }
    const char* title = item["title"] | "";
    if (title[0] == '\0') {
      continue;
    }
    JsonObject event = events.add<JsonObject>();
    event["year"] = item["year"] | "";
    event["title"] = title;
    event["desc"] = item["desc"] | "";
    count++;
  }

  if (count == 0) {
    LOG_ERR("TODAY", "Today history response contains no usable items");
    return false;
  }

  std::string out;
  serializeJson(cacheDoc, out);
  if (out.empty() || out.size() > MAX_TODAY_CACHE_BYTES) {
    LOG_ERR("TODAY", "Today history cache too large: %zu", out.size());
    return false;
  }

  Storage.mkdir("/.crosspoint");
  Storage.mkdir(TODAY_CACHE_DIR);
  return Storage.writeFile(TODAY_CACHE_PATH, String(out.c_str()));
}

bool TodayHistoryClient::loadCached(TodayHistoryInfo& outInfo) {
  outInfo = TodayHistoryInfo{};
  outInfo.events.reserve(MAX_TODAY_EVENTS);
  if (!Storage.exists(TODAY_CACHE_PATH)) {
    return false;
  }

  const String json = Storage.readFile(TODAY_CACHE_PATH);
  if (json.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("TODAY", "Today history cache JSON parse failed: %s", error.c_str());
    return false;
  }

  copyJsonString(outInfo.update, sizeof(outInfo.update), doc["update"]);
  if (doc["events"].is<JsonArrayConst>()) {
    for (JsonObjectConst item : doc["events"].as<JsonArrayConst>()) {
      if (outInfo.events.size() >= MAX_TODAY_EVENTS) {
        break;
      }
      TodayHistoryEvent event;
      copyJsonString(event.year, sizeof(event.year), item["year"]);
      event.title = item["title"] | "";
      event.desc = item["desc"] | "";
      if (!event.title.empty()) {
        outInfo.events.push_back(std::move(event));
      }
    }
  }
  outInfo.hasAny = !outInfo.events.empty();
  return outInfo.hasAny;
}

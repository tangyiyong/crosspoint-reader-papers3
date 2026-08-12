#include "NewsDataClient.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "WifiCredentialStore.h"
#include "network/HttpDownloader.h"

namespace {
constexpr char NEWS_CACHE_DIR[] = "/.crosspoint/news";
constexpr char NEWS_CACHE_PATH[] = "/.crosspoint/news/hot.json";
constexpr char SHWGIJ_HOT_NEWS_API_URL[] = "https://api.shwgij.com/api/news/fastnews/news_rest";
constexpr unsigned long SHWGIJ_NEWS_MIN_REQUEST_INTERVAL_MS = 1500;
constexpr size_t MAX_NEWS_RESPONSE_BYTES = 30000;
constexpr size_t MAX_NEWS_CACHE_BYTES = 26000;
constexpr size_t MAX_NEWS_ITEMS = 20;
unsigned long lastNewsRequestAt = 0;

void copyJsonString(char* dest, const size_t destSize, JsonVariantConst value) {
  if (destSize == 0) return;
  const char* text = value | "";
  snprintf(dest, destSize, "%s", text);
}
}  // namespace

bool NewsDataClient::hasConfiguredApi() { return SETTINGS.calendarApiToken[0] != '\0'; }

bool NewsDataClient::ensureWifiConnectedFromSavedCredential() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  const std::string ssid = WIFI_STORE.getLastConnectedSsid();
  if (ssid.empty()) {
    return false;
  }

  const auto cred = WIFI_STORE.findCredential(ssid);
  if (!cred) {
    return false;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
  if (cred->password.empty()) {
    WiFi.begin(cred->ssid.c_str());
  } else {
    WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
  }

  constexpr unsigned long timeoutMs = 8000;
  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < timeoutMs) {
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

void NewsDataClient::waitForApiRateLimit() {
  const unsigned long now = millis();
  if (lastNewsRequestAt != 0 && now - lastNewsRequestAt < SHWGIJ_NEWS_MIN_REQUEST_INTERVAL_MS) {
    delay(SHWGIJ_NEWS_MIN_REQUEST_INTERVAL_MS - (now - lastNewsRequestAt));
  }
  lastNewsRequestAt = millis();
}

std::string NewsDataClient::buildUrl() {
  std::string url = SHWGIJ_HOT_NEWS_API_URL;
  url += "?key=";
  url += SETTINGS.calendarApiToken;
  return url;
}

bool NewsDataClient::fetchHotNews(const bool allowSavedWifiConnect) {
  if (!hasConfiguredApi()) {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED && (!allowSavedWifiConnect || !ensureWifiConnectedFromSavedCredential())) {
    return false;
  }

  std::string body;
  const std::string url = buildUrl();
  waitForApiRateLimit();
  if (!HttpDownloader::fetchUrl(url, body)) {
    LOG_ERR("NEWS", "Failed to fetch hot news API");
    return false;
  }

  if (body.empty() || body.size() > MAX_NEWS_RESPONSE_BYTES) {
    LOG_ERR("NEWS", "Invalid hot news response size: %zu", body.size());
    return false;
  }

  JsonDocument responseDoc;
  const DeserializationError responseError = deserializeJson(responseDoc, body);
  if (responseError) {
    LOG_ERR("NEWS", "Hot news JSON parse failed: %s", responseError.c_str());
    return false;
  }

  const int code = responseDoc["code"] | 0;
  if (code != 200 && code != 201) {
    LOG_ERR("NEWS", "Hot news API returned code: %d", code);
    return false;
  }

  const JsonObjectConst data = responseDoc["data"].as<JsonObjectConst>();
  if (data.isNull() || !data["news"].is<JsonArrayConst>()) {
    LOG_ERR("NEWS", "Hot news response has no news array");
    return false;
  }

  JsonDocument cacheDoc;
  cacheDoc["update"] = responseDoc["update"] | "";
  cacheDoc["title"] = data["title"] | "";
  cacheDoc["date"] = data["date"] | "";
  cacheDoc["weiyu"] = data["weiyu"] | "";
  JsonArray news = cacheDoc["news"].to<JsonArray>();
  size_t count = 0;
  for (JsonVariantConst item : data["news"].as<JsonArrayConst>()) {
    if (count >= MAX_NEWS_ITEMS) {
      break;
    }
    const char* text = item | "";
    if (text[0] == '\0') {
      continue;
    }
    news.add(text);
    count++;
  }

  if (count == 0) {
    LOG_ERR("NEWS", "Hot news response contains no usable items");
    return false;
  }

  std::string out;
  serializeJson(cacheDoc, out);
  if (out.empty() || out.size() > MAX_NEWS_CACHE_BYTES) {
    LOG_ERR("NEWS", "Hot news cache too large: %zu", out.size());
    return false;
  }

  Storage.mkdir("/.crosspoint");
  Storage.mkdir(NEWS_CACHE_DIR);
  return Storage.writeFile(NEWS_CACHE_PATH, String(out.c_str()));
}

bool NewsDataClient::loadCached(HotNewsInfo& outInfo) {
  outInfo = HotNewsInfo{};
  outInfo.items.reserve(MAX_NEWS_ITEMS);
  if (!Storage.exists(NEWS_CACHE_PATH)) {
    return false;
  }

  const String json = Storage.readFile(NEWS_CACHE_PATH);
  if (json.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("NEWS", "Hot news cache JSON parse failed: %s", error.c_str());
    return false;
  }

  copyJsonString(outInfo.date, sizeof(outInfo.date), doc["date"]);
  copyJsonString(outInfo.title, sizeof(outInfo.title), doc["title"]);
  copyJsonString(outInfo.weiyu, sizeof(outInfo.weiyu), doc["weiyu"]);
  if (doc["news"].is<JsonArrayConst>()) {
    for (JsonVariantConst item : doc["news"].as<JsonArrayConst>()) {
      if (outInfo.items.size() >= MAX_NEWS_ITEMS) {
        break;
      }
      const char* text = item | "";
      if (text[0] != '\0') {
        outInfo.items.emplace_back(text);
      }
    }
  }
  outInfo.hasAny = !outInfo.items.empty();
  return outInfo.hasAny;
}

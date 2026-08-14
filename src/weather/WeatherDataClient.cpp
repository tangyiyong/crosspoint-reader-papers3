#include "WeatherDataClient.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "network/HttpDownloader.h"
#include "util/WifiUtils.h"

namespace {
constexpr char WEATHER_CACHE_DIR[] = "/.crosspoint/weather";
constexpr char SHWGIJ_WEATHER_API_URL[] = "https://api.shwgij.com/api/tianqi/tianqi";
constexpr unsigned long SHWGIJ_WEATHER_MIN_REQUEST_INTERVAL_MS = 2200;
constexpr size_t MAX_WEATHER_RESPONSE_BYTES = 52000;
constexpr size_t MAX_WEATHER_CACHE_BYTES = 18000;
constexpr size_t MAX_WEATHER_ALARMS = 3;
constexpr size_t MAX_WEATHER_HOURLY = 8;
constexpr size_t MAX_WEATHER_DAILY = 7;
constexpr size_t MAX_WEATHER_TIPS = 4;
unsigned long lastWeatherRequestAt = 0;

void copyJsonString(char* dest, const size_t destSize, JsonVariantConst value) {
  if (destSize == 0) return;
  const char* text = value | "";
  snprintf(dest, destSize, "%s", text);
}

std::string cachePath(const int index) {
  char path[64];
  snprintf(path, sizeof(path), "/.crosspoint/weather/weather_%d.json", index);
  return path;
}

std::string urlEncode(const std::string& text) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(text.size() * 3);
  for (const unsigned char c : text) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else if (c == ' ') {
      out += "%20";
    } else {
      out.push_back('%');
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 0x0F]);
    }
  }
  return out;
}

JsonVariantConst objectAt(JsonObjectConst object, const size_t index) {
  size_t current = 0;
  for (JsonPairConst pair : object) {
    if (current == index) {
      return pair.value();
    }
    current++;
  }
  return JsonVariantConst();
}

JsonVariantConst nested(JsonObjectConst object, const char* lower, const char* upper) {
  if (object[lower]) {
    return object[lower];
  }
  return object[upper];
}

void addStringLine(JsonArray array, const std::string& text, size_t& count, const size_t maxCount) {
  if (text.empty() || count >= maxCount) {
    return;
  }
  array.add(text);
  count++;
}

void loadStringArray(JsonVariantConst source, std::vector<WeatherLine>& target, const size_t maxCount) {
  if (!source.is<JsonArrayConst>()) {
    return;
  }
  target.reserve(maxCount);
  for (JsonVariantConst item : source.as<JsonArrayConst>()) {
    if (target.size() >= maxCount) {
      break;
    }
    const char* text = item | "";
    if (text[0] != '\0') {
      target.push_back({text});
    }
  }
}
}  // namespace

bool WeatherDataClient::hasConfiguredApi() { return SETTINGS.calendarApiToken[0] != '\0'; }

bool WeatherDataClient::ensureWifiConnectedFromSavedCredential() {
  return WifiUtils::ensureConnectedFromSavedCredential();
}

void WeatherDataClient::waitForApiRateLimit() {
  const unsigned long now = millis();
  if (lastWeatherRequestAt != 0 && now - lastWeatherRequestAt < SHWGIJ_WEATHER_MIN_REQUEST_INTERVAL_MS) {
    delay(SHWGIJ_WEATHER_MIN_REQUEST_INTERVAL_MS - (now - lastWeatherRequestAt));
  }
  lastWeatherRequestAt = millis();
}

std::string WeatherDataClient::buildUrl(const WeatherLocation& location) {
  std::string url = SHWGIJ_WEATHER_API_URL;
  url += "?key=";
  url += SETTINGS.calendarApiToken;
  url += "&m=2";
  if (!location.province.empty()) {
    url += "&province=";
    url += urlEncode(location.province);
  }
  if (!location.city.empty()) {
    url += "&city=";
    url += urlEncode(location.city);
  }
  if (!location.county.empty()) {
    url += "&county=";
    url += urlEncode(location.county);
  }
  return url;
}

bool WeatherDataClient::fetchWeather(const WeatherLocation& location, const int index, const bool allowSavedWifiConnect) {
  if (!hasConfiguredApi()) {
    return false;
  }
  if (!WifiUtils::isConnected() && (!allowSavedWifiConnect || !ensureWifiConnectedFromSavedCredential())) {
    return false;
  }

  std::string body;
  const std::string url = buildUrl(location);
  waitForApiRateLimit();
  if (!HttpDownloader::fetchUrl(url, body)) {
    LOG_ERR("WEATHER", "Failed to fetch weather API");
    return false;
  }

  if (body.empty() || body.size() > MAX_WEATHER_RESPONSE_BYTES) {
    LOG_ERR("WEATHER", "Invalid weather response size: %zu", body.size());
    return false;
  }

  JsonDocument responseDoc;
  const DeserializationError responseError = deserializeJson(responseDoc, body);
  if (responseError) {
    LOG_ERR("WEATHER", "Weather JSON parse failed: %s", responseError.c_str());
    return false;
  }

  const int code = responseDoc["Code"] | responseDoc["code"] | 0;
  if (code != 200 && code != 201) {
    LOG_ERR("WEATHER", "Weather API returned code: %d", code);
    return false;
  }

  JsonObjectConst data = responseDoc["Data"].as<JsonObjectConst>();
  if (data.isNull()) {
    data = responseDoc["data"].as<JsonObjectConst>();
  }
  if (data.isNull()) {
    LOG_ERR("WEATHER", "Weather response has no data object");
    return false;
  }

  JsonDocument cacheDoc;
  cacheDoc["city"] = data["City"] | data["city"] | WeatherLocationStore::displayName(location).c_str();
  cacheDoc["update"] = responseDoc["Update"] | responseDoc["update"] | "";

  const JsonObjectConst observe = nested(data, "observe", "Observe").as<JsonObjectConst>();
  if (!observe.isNull()) {
    char current[96];
    snprintf(current, sizeof(current), "%s %sC 湿度%s%% %s%s级", observe["weather_short"] | observe["weather"] | "",
             observe["degree"] | "--", observe["humidity"] | "--", observe["wind_direction_name"] | "", observe["wind_power"] | "");
    cacheDoc["current"] = current;
    cacheDoc["currentCode"] = observe["weather_code"] | "";
  }

  const JsonObjectConst air = nested(data, "air", "Air").as<JsonObjectConst>();
  if (!air.isNull()) {
    char airLine[80];
    snprintf(airLine, sizeof(airLine), "AQI %s %s PM2.5 %s", air["aqi"] | "--", air["aqi_name"] | "", air["pm2.5"] | "--");
    cacheDoc["air"] = airLine;
  }

  JsonArray alarmArray = cacheDoc["alarms"].to<JsonArray>();
  size_t alarmCount = 0;
  if (nested(data, "alarm", "Alarm").is<JsonArrayConst>()) {
    for (JsonObjectConst alarm : nested(data, "alarm", "Alarm").as<JsonArrayConst>()) {
      char line[160];
      snprintf(line, sizeof(line), "%s%s %s", alarm["type_name"] | "", alarm["level_name"] | "", alarm["detail"] | "");
      addStringLine(alarmArray, line, alarmCount, MAX_WEATHER_ALARMS);
    }
  }

  JsonArray hourlyArray = cacheDoc["hourly"].to<JsonArray>();
  size_t hourlyCount = 0;
  const JsonObjectConst hourly = nested(data, "forecast_1h", "forecast_1h").as<JsonObjectConst>();
  if (!hourly.isNull()) {
    for (size_t i = 0; i < MAX_WEATHER_HOURLY; ++i) {
      JsonObjectConst hour = objectAt(hourly, i).as<JsonObjectConst>();
      if (hour.isNull()) {
        break;
      }
      const char* rawTime = hour["update_time"] | "";
      const char* hhmm = strlen(rawTime) >= 12 ? rawTime + 8 : rawTime;
      char timeText[6] = "";
      if (strlen(hhmm) >= 4) {
        snprintf(timeText, sizeof(timeText), "%.2s:%.2s", hhmm, hhmm + 2);
      }
      char line[96];
      snprintf(line, sizeof(line), "%s %sC %s %s%s级", timeText, hour["degree"] | "--",
               hour["weather_short"] | hour["weather"] | "", hour["wind_direction"] | "", hour["wind_power"] | "");
      addStringLine(hourlyArray, line, hourlyCount, MAX_WEATHER_HOURLY);
    }
  }

  JsonArray dailyArray = cacheDoc["daily"].to<JsonArray>();
  size_t dailyCount = 0;
  if (nested(data, "forecast_24h", "forecast_24h").is<JsonArrayConst>()) {
    for (JsonObjectConst day : nested(data, "forecast_24h", "forecast_24h").as<JsonArrayConst>()) {
      char line[112];
      snprintf(line, sizeof(line), "%s %s-%sC %s/%s", day["time"] | "", day["min_degree"] | "--",
               day["max_degree"] | "--", day["day_weather_short"] | day["day_weather"] | "",
               day["night_weather_short"] | day["night_weather"] | "");
      addStringLine(dailyArray, line, dailyCount, MAX_WEATHER_DAILY);
      if (dailyCount >= MAX_WEATHER_DAILY) {
        break;
      }
    }
  }

  JsonArray tipsArray = cacheDoc["tips"].to<JsonArray>();
  size_t tipsCount = 0;
  const JsonObjectConst tips = nested(data, "tips", "Tips").as<JsonObjectConst>();
  if (!tips.isNull()) {
    for (JsonPairConst pair : tips) {
      if (tipsCount >= MAX_WEATHER_TIPS) {
        break;
      }
      if (pair.value().is<JsonArrayConst>()) {
        for (JsonVariantConst item : pair.value().as<JsonArrayConst>()) {
          const char* text = item | "";
          addStringLine(tipsArray, text, tipsCount, MAX_WEATHER_TIPS);
          break;
        }
      }
    }
  }

  std::string out;
  serializeJson(cacheDoc, out);
  if (out.empty() || out.size() > MAX_WEATHER_CACHE_BYTES) {
    LOG_ERR("WEATHER", "Weather cache too large: %zu", out.size());
    return false;
  }

  Storage.mkdir("/.crosspoint");
  Storage.mkdir(WEATHER_CACHE_DIR);
  return Storage.writeFile(cachePath(index).c_str(), String(out.c_str()));
}

bool WeatherDataClient::loadCached(const int index, WeatherInfo& outInfo) {
  outInfo = WeatherInfo{};
  outInfo.alarms.reserve(MAX_WEATHER_ALARMS);
  outInfo.hourly.reserve(MAX_WEATHER_HOURLY);
  outInfo.daily.reserve(MAX_WEATHER_DAILY);
  outInfo.tips.reserve(MAX_WEATHER_TIPS);

  const std::string path = cachePath(index);
  if (!Storage.exists(path.c_str())) {
    return false;
  }

  const String json = Storage.readFile(path.c_str());
  if (json.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("WEATHER", "Weather cache JSON parse failed: %s", error.c_str());
    return false;
  }

  copyJsonString(outInfo.city, sizeof(outInfo.city), doc["city"]);
  copyJsonString(outInfo.update, sizeof(outInfo.update), doc["update"]);
  copyJsonString(outInfo.current, sizeof(outInfo.current), doc["current"]);
  copyJsonString(outInfo.currentCode, sizeof(outInfo.currentCode), doc["currentCode"]);
  copyJsonString(outInfo.air, sizeof(outInfo.air), doc["air"]);
  loadStringArray(doc["alarms"], outInfo.alarms, MAX_WEATHER_ALARMS);
  loadStringArray(doc["hourly"], outInfo.hourly, MAX_WEATHER_HOURLY);
  loadStringArray(doc["daily"], outInfo.daily, MAX_WEATHER_DAILY);
  loadStringArray(doc["tips"], outInfo.tips, MAX_WEATHER_TIPS);

  outInfo.hasAny = outInfo.current[0] != '\0' || !outInfo.hourly.empty() || !outInfo.daily.empty();
  return outInfo.hasAny;
}

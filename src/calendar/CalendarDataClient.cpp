#include "CalendarDataClient.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "network/HttpDownloader.h"
#include "util/WifiUtils.h"

namespace {
constexpr char CALENDAR_CACHE_DIR[] = "/.crosspoint/calendar";
constexpr char SHWGIJ_LUNAR_API_URL[] = "https://api.shwgij.com/api/lunars/lunar";
constexpr size_t MAX_CALENDAR_CACHE_BYTES = 50000;
constexpr unsigned long SHWGIJ_MIN_REQUEST_INTERVAL_MS = 1500;
unsigned long lastCalendarRequestAt = 0;

void copyJsonString(char* dest, const size_t destSize, JsonVariantConst value) {
  if (destSize == 0) return;
  const char* text = value | "";
  snprintf(dest, destSize, "%s", text);
}

void replaceAll(std::string& text, const char* pattern, const char* replacement) {
  size_t pos = text.find(pattern);
  const size_t patternLen = strlen(pattern);
  while (pos != std::string::npos) {
    text.replace(pos, patternLen, replacement);
    pos = text.find(pattern, pos + strlen(replacement));
  }
}

JsonVariantConst findDayObject(JsonDocument& doc, const char* date) {
  if (doc["days"].is<JsonArrayConst>()) {
    for (JsonObjectConst day : doc["days"].as<JsonArrayConst>()) {
      const char* dayDate = day["date"] | "";
      if (strcmp(dayDate, date) == 0) {
        return day;
      }
    }
  }

  if (doc["data"].is<JsonArrayConst>()) {
    for (JsonObjectConst day : doc["data"].as<JsonArrayConst>()) {
      const char* dayDate = day["date"] | "";
      if (strcmp(dayDate, date) == 0) {
        return day;
      }
    }
  }

  if (doc["days"].is<JsonObjectConst>()) {
    return doc["days"][date];
  }
  if (doc["data"].is<JsonObjectConst>()) {
    return doc["data"][date];
  }
  return JsonVariantConst();
}

void copyFirstNonEmpty(char* dest, const size_t destSize, JsonVariantConst obj, const char* keyA, const char* keyB) {
  copyJsonString(dest, destSize, obj[keyA]);
  if (dest[0] == '\0') {
    copyJsonString(dest, destSize, obj[keyB]);
  }
}

void populateDayInfoFromGenericObject(CalendarDayInfo& outInfo, JsonVariantConst obj) {
  copyJsonString(outInfo.lunar, sizeof(outInfo.lunar), obj["lunar"]);
  copyJsonString(outInfo.festival, sizeof(outInfo.festival), obj["festival"]);
  copyFirstNonEmpty(outInfo.solarTerm, sizeof(outInfo.solarTerm), obj, "solarTerm", "term");
  copyFirstNonEmpty(outInfo.almanacGood, sizeof(outInfo.almanacGood), obj, "good", "yi");
  copyFirstNonEmpty(outInfo.almanacBad, sizeof(outInfo.almanacBad), obj, "bad", "ji");
}

const char* effectiveCalendarApiUrl() {
  return SETTINGS.calendarApiUrl[0] != '\0' ? SETTINGS.calendarApiUrl : SHWGIJ_LUNAR_API_URL;
}
}  // namespace

bool CalendarDataClient::hasConfiguredApi() {
  return SETTINGS.calendarApiUrl[0] != '\0' || SETTINGS.calendarApiToken[0] != '\0';
}

std::string CalendarDataClient::cachePath(const int year, const int month) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%04d-%02d.json", CALENDAR_CACHE_DIR, year, month);
  return path;
}

bool CalendarDataClient::isShwgijApi() {
  const std::string url = effectiveCalendarApiUrl();
  return url.find("api.shwgij.com/api/lunars/lunar") != std::string::npos;
}

bool CalendarDataClient::ensureWifiConnectedFromSavedCredential() {
  return WifiUtils::ensureConnectedFromSavedCredential();
}

bool CalendarDataClient::hasCachedDay(const int year, const int month, const int day) {
  CalendarDayInfo cached;
  return loadDayInfo(year, month, day, cached);
}

void CalendarDataClient::waitForApiRateLimit() {
  const unsigned long now = millis();
  if (lastCalendarRequestAt != 0 && now - lastCalendarRequestAt < SHWGIJ_MIN_REQUEST_INTERVAL_MS) {
    delay(SHWGIJ_MIN_REQUEST_INTERVAL_MS - (now - lastCalendarRequestAt));
  }
  lastCalendarRequestAt = millis();
}

std::string CalendarDataClient::buildDayUrl(const int year, const int month, const int day) {
  std::string url = effectiveCalendarApiUrl();
  char yearBuf[8];
  char monthBuf[4];
  char dayBuf[4];
  char dateBuf[24];
  snprintf(yearBuf, sizeof(yearBuf), "%04d", year);
  snprintf(monthBuf, sizeof(monthBuf), "%02d", month);
  snprintf(dayBuf, sizeof(dayBuf), "%02d", day);
  snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d000000", year, month, day);

  const bool hasYearPlaceholder = url.find("{year}") != std::string::npos;
  const bool hasMonthPlaceholder = url.find("{month}") != std::string::npos;
  const bool hasDayPlaceholder = url.find("{day}") != std::string::npos;
  const bool hasDatePlaceholder = url.find("{date}") != std::string::npos;
  replaceAll(url, "{year}", yearBuf);
  replaceAll(url, "{month}", monthBuf);
  replaceAll(url, "{day}", dayBuf);
  replaceAll(url, "{date}", dateBuf);

  if (isShwgijApi() || (!hasYearPlaceholder && !hasMonthPlaceholder && !hasDayPlaceholder && !hasDatePlaceholder)) {
    if (isShwgijApi() && url.find("key=") == std::string::npos && SETTINGS.calendarApiToken[0] != '\0') {
      const char sep = url.find('?') == std::string::npos ? '?' : '&';
      url += sep;
      url += "key=";
      url += SETTINGS.calendarApiToken;
    }
    if (url.find("date=") == std::string::npos) {
      const char sep = url.find('?') == std::string::npos ? '?' : '&';
      url += sep;
      url += "date=";
      url += dateBuf;
    } else if (url.rfind("date=") == url.size() - 5) {
      url += dateBuf;
    }
  }
  return url;
}

std::string CalendarDataClient::buildMonthUrl(const int year, const int month) {
  std::string url = effectiveCalendarApiUrl();
  const bool hasYearPlaceholder = url.find("{year}") != std::string::npos;
  const bool hasMonthPlaceholder = url.find("{month}") != std::string::npos;
  if (hasYearPlaceholder) {
    char yearBuf[8];
    snprintf(yearBuf, sizeof(yearBuf), "%04d", year);
    const size_t pos = url.find("{year}");
    url.replace(pos, 6, yearBuf);
  }
  if (hasMonthPlaceholder) {
    char monthBuf[4];
    snprintf(monthBuf, sizeof(monthBuf), "%02d", month);
    const size_t pos = url.find("{month}");
    url.replace(pos, 7, monthBuf);
  }
  if (!hasYearPlaceholder && !hasMonthPlaceholder) {
    const char sep = url.find('?') == std::string::npos ? '?' : '&';
    char suffix[32];
    snprintf(suffix, sizeof(suffix), "%cyear=%04d&month=%02d", sep, year, month);
    url += suffix;
  }
  return url;
}

bool CalendarDataClient::syncMonth(const int year, const int month) {
  if (!hasConfiguredApi() || year <= 0 || month < 1 || month > 12) {
    return false;
  }
  if (!WifiUtils::isConnected() && !ensureWifiConnectedFromSavedCredential()) {
    return false;
  }

  if (isShwgijApi()) {
    struct tm timeinfo;
    const time_t now = time(nullptr);
    if (now > 1700000000) {
      const int offsetQuarterHours = static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48;
      const time_t localNow = now + offsetQuarterHours * 15 * 60;
      gmtime_r(&localNow, &timeinfo);
      if (timeinfo.tm_year + 1900 == year && timeinfo.tm_mon + 1 == month) {
        return syncShwgijDay(year, month, timeinfo.tm_mday);
      }
    }
    return false;
  }

  std::string body;
  const std::string url = buildMonthUrl(year, month);
  if (!HttpDownloader::fetchUrl(url, body)) {
    LOG_ERR("CAL", "Failed to fetch calendar API");
    return false;
  }

  if (body.empty() || body.size() > MAX_CALENDAR_CACHE_BYTES) {
    LOG_ERR("CAL", "Invalid calendar API response size: %zu", body.size());
    return false;
  }

  Storage.mkdir("/.crosspoint");
  Storage.mkdir(CALENDAR_CACHE_DIR);
  const std::string path = cachePath(year, month);
  return Storage.writeFile(path.c_str(), String(body.c_str()));
}

bool CalendarDataClient::syncDay(const int year, const int month, const int day, const bool allowSavedWifiConnect) {
  if (!hasConfiguredApi() || year <= 0 || month < 1 || month > 12 || day < 1 || day > 31) {
    return false;
  }
  if (hasCachedDay(year, month, day)) {
    return true;
  }
  if (!WifiUtils::isConnected() && (!allowSavedWifiConnect || !ensureWifiConnectedFromSavedCredential())) {
    return false;
  }
  if (isShwgijApi()) {
    return syncShwgijDay(year, month, day);
  }
  return syncMonth(year, month);
}

bool CalendarDataClient::syncShwgijDay(const int year, const int month, const int day) {
  if (hasCachedDay(year, month, day)) {
    return true;
  }

  std::string body;
  const std::string url = buildDayUrl(year, month, day);
  waitForApiRateLimit();
  if (!HttpDownloader::fetchUrl(url, body)) {
    LOG_ERR("CAL", "Failed to fetch lunar day API");
    return false;
  }

  if (body.empty() || body.size() > 12000) {
    LOG_ERR("CAL", "Invalid lunar day response size: %zu", body.size());
    return false;
  }

  JsonDocument responseDoc;
  const DeserializationError responseError = deserializeJson(responseDoc, body);
  if (responseError) {
    LOG_ERR("CAL", "Lunar API JSON parse failed: %s", responseError.c_str());
    return false;
  }

  const int code = responseDoc["code"] | 0;
  if (code != 200 && code != 201) {
    LOG_ERR("CAL", "Lunar API returned code: %d", code);
    return false;
  }

  const JsonObjectConst data = responseDoc["data"].as<JsonObjectConst>();
  if (data.isNull()) {
    LOG_ERR("CAL", "Lunar API response has no data object");
    return false;
  }

  JsonDocument cacheDoc;
  const std::string path = cachePath(year, month);
  if (Storage.exists(path.c_str())) {
    const String cached = Storage.readFile(path.c_str());
    if (!cached.isEmpty()) {
      const DeserializationError cacheError = deserializeJson(cacheDoc, cached);
      if (cacheError) {
        LOG_ERR("CAL", "Calendar cache reset after parse failure: %s", cacheError.c_str());
        cacheDoc.clear();
      }
    }
  }

  char dateKey[11];
  snprintf(dateKey, sizeof(dateKey), "%04d-%02d-%02d", year, month, day);
  JsonObject days = cacheDoc["days"].to<JsonObject>();
  JsonObject item = days[dateKey].to<JsonObject>();
  item["date"] = dateKey;
  item["lunar"] = data["Lunar"] | "";
  item["festival"] = data["Festivals"] | "";
  if (strlen(item["festival"] | "") == 0) {
    item["festival"] = data["OtherFestivals"] | "";
  }
  if (strlen(item["festival"] | "") == 0) {
    item["festival"] = data["Lunar_Festivals"] | "";
  }
  if (strlen(item["festival"] | "") == 0) {
    item["festival"] = data["Lunar_OtherFestivals"] | "";
  }
  item["solarTerm"] = data["JieQi1"] | "";
  if (strlen(item["solarTerm"] | "") == 0) {
    item["solarTerm"] = data["SanFu"] | "";
  }
  if (strlen(item["solarTerm"] | "") == 0) {
    item["solarTerm"] = data["ShuJiu"] | "";
  }
  item["good"] = data["YiDay"] | "";
  item["bad"] = data["JiDay"] | "";
  item["solar"] = data["Solar"] | "";
  item["week"] = data["Week"] | "";
  item["constellation"] = data["Constellation"] | "";
  item["lunarYear"] = data["LunarYear"] | "";
  item["ganZhiYear"] = data["GanZhiYear"] | "";
  item["zodiac"] = data["ThisYear"] | "";
  item["quoteShort"] = data["WeiYu_s"] | "";

  std::string out;
  serializeJson(cacheDoc, out);
  if (out.empty() || out.size() > MAX_CALENDAR_CACHE_BYTES) {
    LOG_ERR("CAL", "Merged calendar cache too large: %zu", out.size());
    return false;
  }

  Storage.mkdir("/.crosspoint");
  Storage.mkdir(CALENDAR_CACHE_DIR);
  return Storage.writeFile(path.c_str(), String(out.c_str()));
}

bool CalendarDataClient::loadDayInfo(const int year, const int month, const int day, CalendarDayInfo& outInfo) {
  outInfo = CalendarDayInfo{};
  if (year <= 0 || month < 1 || month > 12 || day < 1 || day > 31) {
    return false;
  }

  char date[11];
  snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);
  snprintf(outInfo.date, sizeof(outInfo.date), "%s", date);

  const std::string path = cachePath(year, month);
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
    LOG_ERR("CAL", "Calendar cache JSON parse failed: %s", error.c_str());
    return false;
  }

  const JsonVariantConst obj = findDayObject(doc, date);
  if (obj.isNull()) {
    return false;
  }

  populateDayInfoFromGenericObject(outInfo, obj);

  outInfo.hasAny = outInfo.lunar[0] != '\0' || outInfo.festival[0] != '\0' || outInfo.solarTerm[0] != '\0' ||
                   outInfo.almanacGood[0] != '\0' || outInfo.almanacBad[0] != '\0';
  return outInfo.hasAny;
}

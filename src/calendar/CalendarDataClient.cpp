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

namespace {
constexpr char CALENDAR_CACHE_DIR[] = "/.crosspoint/calendar";
void copyJsonString(char* dest, const size_t destSize, JsonVariantConst value) {
  if (destSize == 0) return;
  const char* text = value | "";
  snprintf(dest, destSize, "%s", text);
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
}  // namespace

bool CalendarDataClient::hasConfiguredApi() { return SETTINGS.calendarApiUrl[0] != '\0'; }

std::string CalendarDataClient::cachePath(const int year, const int month) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%04d-%02d.json", CALENDAR_CACHE_DIR, year, month);
  return path;
}

std::string CalendarDataClient::buildMonthUrl(const int year, const int month) {
  std::string url = SETTINGS.calendarApiUrl;
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
  if (!hasConfiguredApi() || WiFi.status() != WL_CONNECTED || year <= 0 || month < 1 || month > 12) {
    return false;
  }

  std::string body;
  const std::string url = buildMonthUrl(year, month);
  if (!HttpDownloader::fetchUrl(url, body)) {
    LOG_ERR("CAL", "Failed to fetch calendar API");
    return false;
  }

  if (body.empty() || body.size() > 50000) {
    LOG_ERR("CAL", "Invalid calendar API response size: %zu", body.size());
    return false;
  }

  Storage.mkdir("/.crosspoint");
  Storage.mkdir(CALENDAR_CACHE_DIR);
  const std::string path = cachePath(year, month);
  return Storage.writeFile(path.c_str(), String(body.c_str()));
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

  copyJsonString(outInfo.lunar, sizeof(outInfo.lunar), obj["lunar"]);
  copyJsonString(outInfo.festival, sizeof(outInfo.festival), obj["festival"]);
  copyJsonString(outInfo.solarTerm, sizeof(outInfo.solarTerm), obj["solarTerm"]);
  if (outInfo.solarTerm[0] == '\0') {
    copyJsonString(outInfo.solarTerm, sizeof(outInfo.solarTerm), obj["term"]);
  }
  copyJsonString(outInfo.almanacGood, sizeof(outInfo.almanacGood), obj["good"]);
  if (outInfo.almanacGood[0] == '\0') {
    copyJsonString(outInfo.almanacGood, sizeof(outInfo.almanacGood), obj["yi"]);
  }
  copyJsonString(outInfo.almanacBad, sizeof(outInfo.almanacBad), obj["bad"]);
  if (outInfo.almanacBad[0] == '\0') {
    copyJsonString(outInfo.almanacBad, sizeof(outInfo.almanacBad), obj["ji"]);
  }

  outInfo.hasAny = outInfo.lunar[0] != '\0' || outInfo.festival[0] != '\0' || outInfo.solarTerm[0] != '\0' ||
                   outInfo.almanacGood[0] != '\0' || outInfo.almanacBad[0] != '\0';
  return outInfo.hasAny;
}

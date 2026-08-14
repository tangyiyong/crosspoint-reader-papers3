#include "WeatherLocationStore.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

namespace {
constexpr char WEATHER_DIR[] = "/.crosspoint/weather";
constexpr char WEATHER_LOCATIONS_PATH[] = "/.crosspoint/weather/locations.json";
constexpr size_t MAX_LOCATION_FIELD_BYTES = 40;

std::string limitedText(const char* text) {
  if (!text) {
    return "";
  }
  std::string out(text);
  if (out.size() > MAX_LOCATION_FIELD_BYTES) {
    out.resize(MAX_LOCATION_FIELD_BYTES);
  }
  return out;
}
}  // namespace

bool WeatherLocationStore::load(std::vector<WeatherLocation>& locations) {
  locations.clear();
  locations.reserve(MAX_LOCATIONS);
  if (!Storage.exists(WEATHER_LOCATIONS_PATH)) {
    return false;
  }

  const String json = Storage.readFile(WEATHER_LOCATIONS_PATH);
  if (json.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("WTHLOC", "Location JSON parse failed: %s", error.c_str());
    return false;
  }

  if (!doc["locations"].is<JsonArrayConst>()) {
    return false;
  }

  for (JsonObjectConst item : doc["locations"].as<JsonArrayConst>()) {
    if (locations.size() >= MAX_LOCATIONS) {
      break;
    }
    WeatherLocation location;
    location.province = limitedText(item["province"] | "");
    location.city = limitedText(item["city"] | "");
    location.county = limitedText(item["county"] | "");
    if (!location.province.empty() || !location.city.empty() || !location.county.empty()) {
      locations.push_back(location);
    }
  }
  return !locations.empty();
}

bool WeatherLocationStore::save(const std::vector<WeatherLocation>& locations) {
  JsonDocument doc;
  JsonArray array = doc["locations"].to<JsonArray>();
  size_t count = 0;
  for (const WeatherLocation& location : locations) {
    if (count >= MAX_LOCATIONS) {
      break;
    }
    JsonObject item = array.add<JsonObject>();
    item["province"] = location.province;
    item["city"] = location.city;
    item["county"] = location.county;
    count++;
  }

  std::string out;
  serializeJson(doc, out);
  Storage.mkdir("/.crosspoint");
  Storage.mkdir(WEATHER_DIR);
  return Storage.writeFile(WEATHER_LOCATIONS_PATH, String(out.c_str()));
}

bool WeatherLocationStore::add(const WeatherLocation& location) {
  std::vector<WeatherLocation> locations;
  load(locations);
  if (locations.size() >= MAX_LOCATIONS) {
    return false;
  }
  locations.push_back(location);
  return save(locations);
}

std::string WeatherLocationStore::displayName(const WeatherLocation& location) {
  std::string name;
  if (!location.province.empty()) {
    name += location.province;
  }
  if (!location.city.empty()) {
    if (!name.empty()) name += " ";
    name += location.city;
  }
  if (!location.county.empty()) {
    if (!name.empty()) name += " ";
    name += location.county;
  }
  return name.empty() ? "IP" : name;
}

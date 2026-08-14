#pragma once

#include <string>
#include <vector>

#include "WeatherLocationStore.h"

struct WeatherLine {
  std::string text;
};

struct WeatherInfo {
  char city[64] = "";
  char update[20] = "";
  char current[80] = "";
  char currentCode[8] = "";
  char air[64] = "";
  std::vector<WeatherLine> alarms;
  std::vector<WeatherLine> hourly;
  std::vector<WeatherLine> daily;
  std::vector<WeatherLine> tips;
  bool hasAny = false;
};

class WeatherDataClient {
 public:
  static bool fetchWeather(const WeatherLocation& location, int index, bool allowSavedWifiConnect = true);
  static bool loadCached(int index, WeatherInfo& outInfo);
  static bool hasConfiguredApi();

 private:
  static bool ensureWifiConnectedFromSavedCredential();
  static void waitForApiRateLimit();
  static std::string buildUrl(const WeatherLocation& location);
};

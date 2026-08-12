#pragma once

#include <cstddef>
#include <string>

struct QuoteInfo {
  char date[11] = "";
  char type[32] = "";
  char text[256] = "";
  char cn[160] = "";
  bool hasAny = false;
};

class QuoteDataClient {
 public:
  static bool syncDailyIfNeeded(bool allowSavedWifiConnect = false);
  static bool fetchRandom(bool allowSavedWifiConnect = true);
  static bool loadCached(QuoteInfo& outInfo);
  static bool hasConfiguredApi();

 private:
  static bool ensureWifiConnectedFromSavedCredential();
  static void waitForApiRateLimit();
  static bool fetchAndCache(bool dailyMode);
  static bool isCacheFreshForToday();
  static bool getLocalDate(char* outDate, size_t outDateSize);
  static std::string buildUrl(bool dailyMode);
};

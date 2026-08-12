#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct HotNewsInfo {
  char date[80] = "";
  char title[96] = "";
  char weiyu[160] = "";
  std::vector<std::string> items;
  bool hasAny = false;
};

class NewsDataClient {
 public:
  static bool fetchHotNews(bool allowSavedWifiConnect = true);
  static bool loadCached(HotNewsInfo& outInfo);
  static bool hasConfiguredApi();

 private:
  static bool ensureWifiConnectedFromSavedCredential();
  static void waitForApiRateLimit();
  static std::string buildUrl();
};

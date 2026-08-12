#pragma once

#include <string>
#include <vector>

struct TodayHistoryEvent {
  char year[12] = "";
  std::string title;
  std::string desc;
};

struct TodayHistoryInfo {
  char update[16] = "";
  std::vector<TodayHistoryEvent> events;
  bool hasAny = false;
};

class TodayHistoryClient {
 public:
  static bool syncToday(bool allowSavedWifiConnect = true);
  static bool loadCached(TodayHistoryInfo& outInfo);
  static bool hasConfiguredApi();

 private:
  static bool ensureWifiConnectedFromSavedCredential();
  static void waitForApiRateLimit();
  static std::string buildUrl();
};

#pragma once

#include <string>

struct CalendarDayInfo {
  char date[11] = "";
  char lunar[48] = "";
  char festival[48] = "";
  char solarTerm[32] = "";
  char almanacGood[80] = "";
  char almanacBad[80] = "";
  bool hasAny = false;
};

class CalendarDataClient {
 public:
  static bool syncMonth(int year, int month);
  static bool loadDayInfo(int year, int month, int day, CalendarDayInfo& outInfo);
  static bool hasConfiguredApi();

 private:
  static std::string buildMonthUrl(int year, int month);
  static std::string cachePath(int year, int month);
};

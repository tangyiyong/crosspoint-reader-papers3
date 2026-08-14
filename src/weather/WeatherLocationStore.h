#pragma once

#include <string>
#include <vector>

struct WeatherLocation {
  std::string province;
  std::string city;
  std::string county;
};

class WeatherLocationStore {
 public:
  static constexpr size_t MAX_LOCATIONS = 10;

  static bool load(std::vector<WeatherLocation>& locations);
  static bool save(const std::vector<WeatherLocation>& locations);
  static bool add(const WeatherLocation& location);
  static std::string displayName(const WeatherLocation& location);
};

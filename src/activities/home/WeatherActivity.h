#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"
#include "weather/WeatherDataClient.h"

class WeatherActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  std::vector<WeatherLocation> locations;
  WeatherInfo weather;
  WeatherLocation draftLocation;
  int selectedLocation = 0;
  int topLine = 0;
  uint16_t deleteMask = 0;
  bool syncing = false;
  bool lastFetchFailed = false;
  bool autoAttempted = false;
  bool deleteMode = false;

  void loadLocations();
  void loadSelectedWeather();
  void refreshAll();
  void maybeConnectWifiAndRefresh();
  void addLocation();
  void promptProvince();
  void promptCity(int provinceIndex);
  void nextLocation();
  void deleteSelectedLocation();
  void enterDeleteMode();
  void toggleDeleteSelection(int index);
  void confirmDeleteSelection();
  int hitTestLocationTab(int touchX, int touchY) const;
  int visibleLineCount() const;
  int totalLineCount() const;
  std::string lineAt(int index) const;
  void drawWeatherIcon(int x, int y, int size, const char* code) const;
  void drawDeleteMode();

 public:
  explicit WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Weather", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};

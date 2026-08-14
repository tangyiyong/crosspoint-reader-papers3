#include "WeatherActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <array>
#include <variant>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ListSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/WifiUtils.h"
#include "weather/WeatherLocationStore.h"

namespace {
struct WeatherCityOption {
  const char* city;
  const char* county;
};

struct WeatherProvinceOption {
  const char* province;
  const WeatherCityOption* cities;
  size_t cityCount;
};

constexpr WeatherCityOption BEIJING_CITIES[] = {{"北京", "东城"}, {"北京", "朝阳"}, {"北京", "海淀"}, {"北京", "通州"}};
constexpr WeatherCityOption SHANGHAI_CITIES[] = {{"上海", "黄浦"}, {"上海", "浦东"}, {"上海", "徐汇"}, {"上海", "闵行"}};
constexpr WeatherCityOption GUANGDONG_CITIES[] = {{"广州", "越秀"}, {"深圳", "南山"}, {"佛山", "禅城"}, {"东莞", "东城"}};
constexpr WeatherCityOption GUANGXI_CITIES[] = {{"南宁", "青秀"}, {"桂林", "秀峰"}, {"玉林", "博白"}, {"柳州", "城中"}};
constexpr WeatherCityOption SHANDONG_CITIES[] = {{"济南", "历下"}, {"青岛", "崂山"}, {"烟台", "芝罘"}, {"潍坊", "奎文"}};
constexpr WeatherCityOption JIANGSU_CITIES[] = {{"南京", "玄武"}, {"苏州", "姑苏"}, {"无锡", "梁溪"}, {"扬州", "广陵"}};
constexpr WeatherCityOption ZHEJIANG_CITIES[] = {{"杭州", "西湖"}, {"宁波", "海曙"}, {"温州", "鹿城"}, {"绍兴", "越城"}};
constexpr WeatherCityOption SICHUAN_CITIES[] = {{"成都", "锦江"}, {"绵阳", "涪城"}, {"乐山", "市中"}, {"宜宾", "翠屏"}};
constexpr WeatherCityOption HUBEI_CITIES[] = {{"武汉", "武昌"}, {"宜昌", "西陵"}, {"襄阳", "襄城"}, {"荆州", "沙市"}};
constexpr WeatherCityOption HUNAN_CITIES[] = {{"长沙", "芙蓉"}, {"株洲", "天元"}, {"衡阳", "石鼓"}, {"岳阳", "岳阳楼"}};
constexpr WeatherCityOption FUJIAN_CITIES[] = {{"福州", "鼓楼"}, {"厦门", "思明"}, {"泉州", "鲤城"}, {"漳州", "芗城"}};
constexpr WeatherCityOption HENAN_CITIES[] = {{"郑州", "中原"}, {"洛阳", "西工"}, {"开封", "龙亭"}, {"南阳", "卧龙"}};
constexpr WeatherCityOption SHAANXI_CITIES[] = {{"西安", "碑林"}, {"咸阳", "秦都"}, {"宝鸡", "金台"}, {"渭南", "临渭"}};

constexpr WeatherProvinceOption WEATHER_PROVINCES[] = {
    {"北京", BEIJING_CITIES, std::size(BEIJING_CITIES)},     {"上海", SHANGHAI_CITIES, std::size(SHANGHAI_CITIES)},
    {"广东", GUANGDONG_CITIES, std::size(GUANGDONG_CITIES)}, {"广西", GUANGXI_CITIES, std::size(GUANGXI_CITIES)},
    {"山东", SHANDONG_CITIES, std::size(SHANDONG_CITIES)},   {"江苏", JIANGSU_CITIES, std::size(JIANGSU_CITIES)},
    {"浙江", ZHEJIANG_CITIES, std::size(ZHEJIANG_CITIES)},   {"四川", SICHUAN_CITIES, std::size(SICHUAN_CITIES)},
    {"湖北", HUBEI_CITIES, std::size(HUBEI_CITIES)},         {"湖南", HUNAN_CITIES, std::size(HUNAN_CITIES)},
    {"福建", FUJIAN_CITIES, std::size(FUJIAN_CITIES)},       {"河南", HENAN_CITIES, std::size(HENAN_CITIES)},
    {"陕西", SHAANXI_CITIES, std::size(SHAANXI_CITIES)},
};

std::vector<std::string> provinceNames() {
  std::vector<std::string> names;
  names.reserve(std::size(WEATHER_PROVINCES));
  for (const auto& province : WEATHER_PROVINCES) {
    names.emplace_back(province.province);
  }
  return names;
}

std::vector<std::string> cityNames(const int provinceIndex) {
  std::vector<std::string> names;
  if (provinceIndex < 0 || provinceIndex >= static_cast<int>(std::size(WEATHER_PROVINCES))) {
    return names;
  }
  const auto& province = WEATHER_PROVINCES[provinceIndex];
  names.reserve(province.cityCount);
  for (size_t i = 0; i < province.cityCount; ++i) {
    std::string label = province.cities[i].city;
    label += " ";
    label += province.cities[i].county;
    names.push_back(label);
  }
  return names;
}
}  // namespace

void WeatherActivity::onEnter() {
  Activity::onEnter();
  loadLocations();
  loadSelectedWeather();
  requestUpdate();
}

void WeatherActivity::loadLocations() {
  WeatherLocationStore::load(locations);
  if (!locations.empty()) {
    selectedLocation = std::clamp(selectedLocation, 0, static_cast<int>(locations.size()) - 1);
  } else {
    selectedLocation = 0;
  }
}

void WeatherActivity::loadSelectedWeather() {
  if (locations.empty()) {
    weather = WeatherInfo{};
    topLine = 0;
    return;
  }
  WeatherDataClient::loadCached(selectedLocation, weather);
  topLine = std::min(topLine, std::max(0, totalLineCount() - visibleLineCount()));
}

void WeatherActivity::maybeConnectWifiAndRefresh() {
  if (autoAttempted) {
    return;
  }
  autoAttempted = true;
  if (locations.empty()) {
    requestUpdate();
    return;
  }
  if (WifiUtils::isConnected()) {
    refreshAll();
    return;
  }

  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true),
                         [this](const ActivityResult& result) {
                           if (!result.isCancelled && std::holds_alternative<WifiResult>(result.data) &&
                               std::get<WifiResult>(result.data).connected) {
                             refreshAll();
                           } else {
                             requestUpdate();
                           }
                         });
}

void WeatherActivity::refreshAll() {
  if (locations.empty()) {
    return;
  }
  syncing = true;
  lastFetchFailed = false;
  requestUpdateAndWait();

  bool anyFailed = false;
  for (size_t i = 0; i < locations.size() && i < WeatherLocationStore::MAX_LOCATIONS; ++i) {
    if (!WeatherDataClient::fetchWeather(locations[i], static_cast<int>(i), true)) {
      anyFailed = true;
    }
  }

  lastFetchFailed = anyFailed;
  loadSelectedWeather();
  syncing = false;
  requestUpdate();
}

void WeatherActivity::addLocation() {
  if (locations.size() >= WeatherLocationStore::MAX_LOCATIONS) {
    return;
  }
  draftLocation = WeatherLocation{};
  promptProvince();
}

void WeatherActivity::promptProvince() {
  startActivityForResult(std::make_unique<ListSelectionActivity>(renderer, mappedInput, tr(STR_WEATHER_PROVINCE),
                                                                provinceNames()),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled || !std::holds_alternative<MenuResult>(result.data)) {
                             requestUpdate();
                             return;
                           }
                           const int provinceIndex = std::get<MenuResult>(result.data).action;
                           if (provinceIndex < 0 ||
                               provinceIndex >= static_cast<int>(std::size(WEATHER_PROVINCES))) {
                             requestUpdate();
                             return;
                           }
                           draftLocation.province = WEATHER_PROVINCES[provinceIndex].province;
                           promptCity(provinceIndex);
                         });
}

void WeatherActivity::promptCity(const int provinceIndex) {
  startActivityForResult(std::make_unique<ListSelectionActivity>(renderer, mappedInput, tr(STR_WEATHER_CITY),
                                                                cityNames(provinceIndex)),
                         [this, provinceIndex](const ActivityResult& result) {
                           if (result.isCancelled || !std::holds_alternative<MenuResult>(result.data)) {
                             requestUpdate();
                             return;
                           }
                           if (provinceIndex < 0 ||
                               provinceIndex >= static_cast<int>(std::size(WEATHER_PROVINCES))) {
                             requestUpdate();
                             return;
                           }
                           const auto& province = WEATHER_PROVINCES[provinceIndex];
                           const int cityIndex = std::get<MenuResult>(result.data).action;
                           if (cityIndex < 0 || cityIndex >= static_cast<int>(province.cityCount)) {
                             requestUpdate();
                             return;
                           }
                           draftLocation.city = province.cities[cityIndex].city;
                           draftLocation.county = province.cities[cityIndex].county;
                           if (WeatherLocationStore::add(draftLocation)) {
                             loadLocations();
                             selectedLocation = static_cast<int>(locations.size()) - 1;
                             loadSelectedWeather();
                           }
                           requestUpdate();
                         });
}

void WeatherActivity::nextLocation() {
  if (locations.empty()) {
    return;
  }
  selectedLocation = ButtonNavigator::nextIndex(selectedLocation, static_cast<int>(locations.size()));
  topLine = 0;
  loadSelectedWeather();
  requestUpdate();
}

void WeatherActivity::deleteSelectedLocation() {
  if (locations.empty()) {
    return;
  }
  enterDeleteMode();
}

void WeatherActivity::enterDeleteMode() {
  deleteMode = true;
  deleteMask = locations.empty() ? 0 : static_cast<uint16_t>(1U << selectedLocation);
  requestUpdate();
}

void WeatherActivity::toggleDeleteSelection(const int index) {
  if (index < 0 || index >= static_cast<int>(locations.size()) || index >= 16) {
    return;
  }
  deleteMask ^= static_cast<uint16_t>(1U << index);
  selectedLocation = index;
  requestUpdate();
}

void WeatherActivity::confirmDeleteSelection() {
  if (deleteMask == 0) {
    deleteMode = false;
    requestUpdate();
    return;
  }
  std::vector<WeatherLocation> kept;
  kept.reserve(locations.size());
  for (int i = 0; i < static_cast<int>(locations.size()); ++i) {
    if ((deleteMask & (1U << i)) == 0) {
      kept.push_back(locations[i]);
    }
  }
  locations = std::move(kept);
  selectedLocation = std::min(selectedLocation, std::max(0, static_cast<int>(locations.size()) - 1));
  deleteMask = 0;
  deleteMode = false;
  WeatherLocationStore::save(locations);
  loadSelectedWeather();
  requestUpdate();
}

int WeatherActivity::hitTestLocationTab(const int touchX, const int touchY) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int rowY = top;
  const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + 14;
  if (locations.empty() || touchY < rowY || touchY >= rowY + rowH) {
    return -1;
  }
  const int cellW = std::max(1, pageWidth / static_cast<int>(locations.size()));
  const int index = std::min(static_cast<int>(locations.size()) - 1, touchX / cellW);
  return index;
}

int WeatherActivity::visibleLineCount() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + 86;
  const int bottom = pageHeight - metrics.buttonHintsHeight - 12;
  return std::max(1, (bottom - top) / (renderer.getLineHeight(UI_10_FONT_ID) + 5));
}

int WeatherActivity::totalLineCount() const {
  int total = 4;
  total += static_cast<int>(weather.alarms.size());
  total += static_cast<int>(weather.hourly.size());
  total += static_cast<int>(weather.daily.size());
  total += static_cast<int>(weather.tips.size());
  return total;
}

std::string WeatherActivity::lineAt(const int index) const {
  int current = 0;
  if (index == current++) return std::string(tr(STR_WEATHER_NOW)) + ": " + (weather.current[0] ? weather.current : "--");
  if (index == current++) return std::string(tr(STR_WEATHER_AIR)) + ": " + (weather.air[0] ? weather.air : "--");
  if (index == current++) return std::string(tr(STR_WEATHER_ALARM)) + ":";
  for (const WeatherLine& line : weather.alarms) {
    if (index == current++) return std::string("! ") + line.text;
  }
  if (index == current++) return std::string(tr(STR_WEATHER_HOURLY)) + ":";
  for (const WeatherLine& line : weather.hourly) {
    if (index == current++) return line.text;
  }
  if (index == current++) return std::string(tr(STR_WEATHER_DAILY)) + ":";
  for (const WeatherLine& line : weather.daily) {
    if (index == current++) return line.text;
  }
  if (index == current++) return std::string(tr(STR_WEATHER_TIPS)) + ":";
  for (const WeatherLine& line : weather.tips) {
    if (index == current++) return line.text;
  }
  return "";
}

void WeatherActivity::drawWeatherIcon(const int x, const int y, const int size, const char* code) const {
  const bool rain = code && (code[0] == '0' && (code[1] == '4' || code[1] == '7' || code[1] == '8' || code[1] == '9'));
  const bool snow = code && code[0] == '1' && (code[1] == '3' || code[1] == '4' || code[1] == '5' || code[1] == '6');
  const bool sunny = code && code[0] == '0' && code[1] == '0';
  const int cx = x + size / 2;
  const int cy = y + size / 2;
  const int q = size / 4;

  if (sunny) {
    renderer.drawRect(cx - q / 2, cy - q / 2, q, q, true);
    renderer.drawLine(cx, y + 2, cx, y + q, true);
    renderer.drawLine(cx, y + size - q, cx, y + size - 2, true);
    renderer.drawLine(x + 2, cy, x + q, cy, true);
    renderer.drawLine(x + size - q, cy, x + size - 2, cy, true);
    renderer.drawLine(x + q, y + q, x + q / 2, y + q / 2, true);
    renderer.drawLine(x + size - q, y + q, x + size - q / 2, y + q / 2, true);
    return;
  }

  renderer.drawRect(x + q / 2, y + q, size - q, q + 3, true);
  renderer.drawLine(x + q, y + q, x + q + 5, y + q / 2, true);
  renderer.drawLine(x + q + 5, y + q / 2, cx, y + q, true);
  renderer.drawLine(cx, y + q, x + size - q, y + q + 2, true);

  if (rain) {
    renderer.drawLine(x + q, y + size - q, x + q - 3, y + size - 3, true);
    renderer.drawLine(cx, y + size - q, cx - 3, y + size - 3, true);
    renderer.drawLine(x + size - q, y + size - q, x + size - q - 3, y + size - 3, true);
  } else if (snow) {
    renderer.drawLine(cx, y + size - q, cx, y + size - 3, true);
    renderer.drawLine(cx - 4, y + size - 6, cx + 4, y + size - 6, true);
    renderer.drawLine(cx - 3, y + size - 9, cx + 3, y + size - 3, true);
  }
}

void WeatherActivity::loop() {
  if (!autoAttempted) {
    maybeConnectWifiAndRefresh();
    return;
  }

  if (deleteMode) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
      deleteMode = false;
      deleteMask = 0;
      requestUpdate();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      confirmDeleteSelection();
      return;
    }
    if (mappedInput.wasContentTapped()) {
      toggleDeleteSelection(hitTestLocationTab(mappedInput.getTouchX(), mappedInput.getTouchY()));
      return;
    }
    buttonNavigator.onNextRelease([this] {
      if (!locations.empty()) {
        selectedLocation = ButtonNavigator::nextIndex(selectedLocation, static_cast<int>(locations.size()));
        requestUpdate();
      }
    });
    buttonNavigator.onPreviousRelease([this] {
      if (!locations.empty()) {
        selectedLocation = ButtonNavigator::previousIndex(selectedLocation, static_cast<int>(locations.size()));
        requestUpdate();
      }
    });
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasEdgeBackGesture()) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    refreshAll();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    addLocation();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    deleteSelectedLocation();
    return;
  }

  if (mappedInput.wasContentTapped()) {
    const int pageW = renderer.getScreenWidth();
    const int pageH = renderer.getScreenHeight();
    if (mappedInput.getTouchY() < pageH / 4) {
      nextLocation();
      return;
    }
    if (mappedInput.getTouchX() > pageW * 2 / 3 && mappedInput.getTouchY() > pageH * 3 / 4) {
      addLocation();
      return;
    }
  }

  const int maxTop = std::max(0, totalLineCount() - visibleLineCount());
  if (mappedInput.wasContentSwipedUp()) {
    topLine = std::min(maxTop, topLine + visibleLineCount());
    requestUpdate();
    return;
  }
  if (mappedInput.wasContentSwipedDown()) {
    topLine = std::max(0, topLine - visibleLineCount());
    requestUpdate();
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this, maxTop] {
    topLine = std::min(maxTop, topLine + 1);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    topLine = std::max(0, topLine - 1);
    requestUpdate();
  });
}

void WeatherActivity::render(RenderLock&&) {
  if (deleteMode) {
    drawDeleteMode();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentX = metrics.contentSidePadding;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_WEATHER));

  const std::string locationName =
      locations.empty() ? std::string(tr(STR_APP_WEATHER)) : WeatherLocationStore::displayName(locations[selectedLocation]);
  const std::string safeTitle = renderer.truncatedText(UI_12_FONT_ID, locationName.c_str(), contentW, EpdFontFamily::BOLD);
  const int titleX = weather.currentCode[0] != '\0' ? contentX + 38 : contentX;
  if (weather.currentCode[0] != '\0') {
    drawWeatherIcon(contentX, top - 2, 30, weather.currentCode);
  }
  renderer.drawText(UI_12_FONT_ID, titleX, top, safeTitle.c_str(), true, EpdFontFamily::BOLD);

  const char* status = syncing ? tr(STR_LOADING) : (lastFetchFailed ? tr(STR_WEATHER_FETCH_FAILED) : weather.update);
  if ((!status || status[0] == '\0') && !WifiUtils::isConnected()) {
    status = tr(STR_WEATHER_WIFI_HINT);
  }
  const std::string safeStatus = renderer.truncatedText(UI_10_FONT_ID, status, contentW);
  renderer.drawText(UI_10_FONT_ID, contentX, top + 34, safeStatus.c_str());

  const int listTop = top + 76;
  const int listBottom = pageHeight - metrics.buttonHintsHeight - 12;
  int y = listTop;
  if (!weather.hasAny && !syncing) {
    const auto lines = renderer.wrappedText(UI_10_FONT_ID, tr(STR_WEATHER_EMPTY), contentW, visibleLineCount());
    for (const auto& line : lines) {
      renderer.drawText(UI_10_FONT_ID, contentX, y, line.c_str());
      y += renderer.getLineHeight(UI_10_FONT_ID) + 5;
    }
  } else {
    const int end = std::min(totalLineCount(), topLine + visibleLineCount());
    for (int i = topLine; i < end; ++i) {
      const std::string line = lineAt(i);
      const auto wrapped = renderer.wrappedText(UI_10_FONT_ID, line.c_str(), contentW, 1);
      if (!wrapped.empty()) {
        renderer.drawText(UI_10_FONT_ID, contentX, y, wrapped[0].c_str());
      }
      y += renderer.getLineHeight(UI_10_FONT_ID) + 5;
      if (y > listBottom) {
        break;
      }
    }
  }

  if (totalLineCount() > visibleLineCount()) {
    const int scrollX = pageWidth - metrics.contentSidePadding / 2;
    const int scrollH = listBottom - listTop;
    const int maxTop = std::max(1, totalLineCount() - visibleLineCount());
    const int thumbH = std::max(12, scrollH * visibleLineCount() / totalLineCount());
    const int thumbY = listTop + (scrollH - thumbH) * topLine / maxTop;
    renderer.drawLine(scrollX, listTop, scrollX, listBottom, true);
    renderer.fillRect(scrollX - 2, thumbY, 3, thumbH, true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_ADD), tr(STR_REMOVE));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void WeatherActivity::drawDeleteMode() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int contentX = metrics.contentSidePadding;
  const int contentW = pageWidth - metrics.contentSidePadding * 2;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_REMOVE));

  if (locations.empty()) {
    renderer.drawText(UI_10_FONT_ID, contentX, y, tr(STR_WEATHER_EMPTY));
  } else {
    const int rowH = renderer.getLineHeight(UI_12_FONT_ID) + 14;
    const int rowW = contentW;
    for (int i = 0; i < static_cast<int>(locations.size()); ++i) {
      const bool checked = (deleteMask & (1U << i)) != 0;
      const bool selected = i == selectedLocation;
      renderer.fillRect(contentX, y, rowW, rowH, selected);
      renderer.drawRect(contentX, y, rowW, rowH, !selected);
      renderer.drawRect(contentX + 8, y + 8, 12, 12, !selected);
      if (checked) {
        renderer.drawLine(contentX + 10, y + 14, contentX + 14, y + 18, !selected);
        renderer.drawLine(contentX + 14, y + 18, contentX + 21, y + 9, !selected);
      }
      const std::string label =
          renderer.truncatedText(UI_10_FONT_ID, WeatherLocationStore::displayName(locations[i]).c_str(), rowW - 38);
      renderer.drawText(UI_10_FONT_ID, contentX + 30,
                        y + (rowH - renderer.getLineHeight(UI_10_FONT_ID)) / 2, label.c_str(), !selected);
      y += rowH + 4;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

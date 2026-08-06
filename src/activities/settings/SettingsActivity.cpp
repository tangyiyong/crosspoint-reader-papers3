#include "SettingsActivity.h"

#include <ExternalFont.h>
#include <FontManager.h>
#include <GfxRenderer.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <variant>

#include "ButtonRemapActivity.h"
#include "ClearCacheActivity.h"
#include "CrossPointSettings.h"
#include "FontSelectActivity.h"
#include "KOReaderSettingsActivity.h"
#include "LanguageSelectActivity.h"
#include "LineSpacingSelectionActivity.h"
#include "MappedInputManager.h"
#include "OpdsServerListActivity.h"
#include "OrientationHelper.h"
#include "OtaUpdateActivity.h"
#include "SdFirmwareUpdateActivity.h"
#include "SettingsList.h"
#include "StatusBarSettingsActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ExternalFontLabel.h"

const StrId SettingsActivity::categoryNames[categoryCount] = {StrId::STR_CAT_DISPLAY, StrId::STR_CAT_READER,
                                                              StrId::STR_CAT_CONTROLS, StrId::STR_CAT_SYSTEM};

void SettingsActivity::onEnter() {
  Activity::onEnter();

  // Build per-category vectors from the shared settings list
  displaySettings.clear();
  readerSettings.clear();
  controlsSettings.clear();
  systemSettings.clear();

  for (const auto& setting : getSettingsList()) {
    if (setting.category == StrId::STR_NONE_OPT) continue;
#if CROSSPOINT_PAPERS3
    if (setting.nameId == StrId::STR_FONT_FAMILY) continue;
#endif
    if (setting.category == StrId::STR_CAT_DISPLAY) {
      displaySettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_READER) {
      readerSettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_CONTROLS) {
      controlsSettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_SYSTEM) {
      systemSettings.push_back(setting);
    }
    // Web-only categories (KOReader Sync, OPDS Browser) are skipped for device UI
  }

  // Append device-only ACTION items
  controlsSettings.insert(controlsSettings.begin(),
                          SettingInfo::Action(StrId::STR_REMAP_FRONT_BUTTONS, SettingAction::RemapFrontButtons));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_WIFI_NETWORKS, SettingAction::Network));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_KOREADER_SYNC, SettingAction::KOReaderSync));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_OPDS_BROWSER, SettingAction::OPDSBrowser));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_CLEAR_READING_CACHE, SettingAction::ClearCache));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_CHECK_UPDATES, SettingAction::CheckForUpdates));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_INSTALL_FIRMWARE_SD, SettingAction::InstallFirmwareFromSd));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_LANGUAGE, SettingAction::Language));
#if CROSSPOINT_PAPERS3
  readerSettings.insert(readerSettings.begin(),
                        SettingInfo::Action(StrId::STR_EXTERNAL_FONT, SettingAction::ExternalReaderFont));
#endif
  readerSettings.push_back(SettingInfo::Action(StrId::STR_CUSTOMISE_STATUS_BAR, SettingAction::CustomiseStatusBar));

  // Reset selection to first category
  selectedCategoryIndex = 0;
  selectedSettingIndex = 0;

  // Initialize with first category (Display)
  currentSettings = &displaySettings;
  settingsCount = static_cast<int>(displaySettings.size());

  // Trigger first update
  requestUpdate();
}

void SettingsActivity::onExit() {
  Activity::onExit();

  UITheme::getInstance().reload();  // Re-apply theme in case it was changed
}

void SettingsActivity::loop() {
  bool hasChangedCategory = false;

  // Handle actions with early return
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selectedSettingIndex == 0) {
      selectedCategoryIndex = (selectedCategoryIndex < categoryCount - 1) ? (selectedCategoryIndex + 1) : 0;
      hasChangedCategory = true;
      requestUpdate();
    } else {
      toggleCurrentSetting();
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (selectedSettingIndex > 0) {
      selectedSettingIndex = 0;
      requestUpdate();
    } else {
      SETTINGS.saveToFile();
      onGoHome();
    }
    return;
  }

  // Handle navigation
  buttonNavigator.onNextRelease([this] {
    selectedSettingIndex = ButtonNavigator::nextIndex(selectedSettingIndex, settingsCount + 1);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedSettingIndex = ButtonNavigator::previousIndex(selectedSettingIndex, settingsCount + 1);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, &hasChangedCategory] {
    hasChangedCategory = true;
    selectedCategoryIndex = ButtonNavigator::nextIndex(selectedCategoryIndex, categoryCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, &hasChangedCategory] {
    hasChangedCategory = true;
    selectedCategoryIndex = ButtonNavigator::previousIndex(selectedCategoryIndex, categoryCount);
    requestUpdate();
  });

  if (hasChangedCategory) {
    selectedSettingIndex = (selectedSettingIndex == 0) ? 0 : 1;
    switch (selectedCategoryIndex) {
      case 0:
        currentSettings = &displaySettings;
        break;
      case 1:
        currentSettings = &readerSettings;
        break;
      case 2:
        currentSettings = &controlsSettings;
        break;
      case 3:
        currentSettings = &systemSettings;
        break;
    }
    settingsCount = static_cast<int>(currentSettings->size());
  }
}

void SettingsActivity::toggleCurrentSetting() {
  int selectedSetting = selectedSettingIndex - 1;
  if (selectedSetting < 0 || selectedSetting >= settingsCount) {
    return;
  }

  const auto& setting = (*currentSettings)[selectedSetting];

  if (setting.nameId == StrId::STR_LINE_SPACING) {
    startActivityForResult(
        std::make_unique<LineSpacingSelectionActivity>(renderer, mappedInput, static_cast<int>(SETTINGS.lineSpacing)),
        [this](const ActivityResult& result) {
          if (result.isCancelled || !std::holds_alternative<PercentResult>(result.data)) {
            return;
          }
          const int selected = std::get<PercentResult>(result.data).percent;
          const uint8_t newValue =
              static_cast<uint8_t>(std::clamp(selected, static_cast<int>(CrossPointSettings::LINE_SPACING_MIN),
                                              static_cast<int>(CrossPointSettings::LINE_SPACING_MAX)));
          if (newValue == SETTINGS.lineSpacing) {
            return;
          }
          SETTINGS.lineSpacing = newValue;
          SETTINGS.saveToFile();
          requestUpdate();
        });
    return;
  }

  if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
    // Toggle the boolean value using the member pointer
    const bool currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = !currentValue;
  } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
#if CROSSPOINT_PAPERS3
    if (setting.valuePtr == &CrossPointSettings::orientation) {
      const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
      SETTINGS.*(setting.valuePtr) = CrossPointSettings::nextPaperS3Orientation(currentValue);
    } else
#endif
    {
      const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
      SETTINGS.*(setting.valuePtr) = (currentValue + 1) % static_cast<uint8_t>(setting.enumValues.size());
    }
  } else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
    const int8_t currentValue = SETTINGS.*(setting.valuePtr);
    if (currentValue + setting.valueRange.step > setting.valueRange.max) {
      SETTINGS.*(setting.valuePtr) = setting.valueRange.min;
    } else {
      SETTINGS.*(setting.valuePtr) = currentValue + setting.valueRange.step;
    }
  } else if (setting.type == SettingType::ACTION) {
    auto resultHandler = [this](const ActivityResult&) { SETTINGS.saveToFile(); };

    switch (setting.action) {
      case SettingAction::RemapFrontButtons:
        startActivityForResult(std::make_unique<ButtonRemapActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::CustomiseStatusBar:
        startActivityForResult(std::make_unique<StatusBarSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::KOReaderSync:
        startActivityForResult(std::make_unique<KOReaderSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::OPDSBrowser:
        startActivityForResult(std::make_unique<OpdsServerListActivity>(renderer, mappedInput, false), resultHandler);
        break;
      case SettingAction::Network:
        startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, false), resultHandler);
        break;
      case SettingAction::ClearCache:
        startActivityForResult(std::make_unique<ClearCacheActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::CheckForUpdates:
        startActivityForResult(std::make_unique<OtaUpdateActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::InstallFirmwareFromSd:
        startActivityForResult(std::make_unique<SdFirmwareUpdateActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::Language:
        startActivityForResult(std::make_unique<LanguageSelectActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::ExternalReaderFont:
        startActivityForResult(std::make_unique<FontSelectActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::None:
        // Do nothing
        break;
    }
    return;  // Results will be handled in the result handler, so we can return early here
  } else {
    return;
  }

  if (setting.nameId == StrId::STR_COLOR_MODE) {
    renderer.setDarkMode(SETTINGS.colorMode == CrossPointSettings::COLOR_MODE::DARK_MODE);
  } else if (setting.nameId == StrId::STR_UI_ORIENTATION) {
#if CROSSPOINT_PAPERS3
    OrientationHelper::applyOrientation(renderer, mappedInput, this);
#endif
  } else if (setting.nameId == StrId::STR_INVERT_IMAGES) {
    renderer.setInvertImagesInDarkMode(SETTINGS.invertImages);
  }

  SETTINGS.saveToFile();
}

void SettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SETTINGS_TITLE),
                 CROSSPOINT_VERSION);

  std::vector<TabInfo> tabs;
  tabs.reserve(categoryCount);
  for (int i = 0; i < categoryCount; i++) {
    tabs.push_back({I18N.get(categoryNames[i]), selectedCategoryIndex == i});
  }
  GUI.drawTabBar(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight}, tabs,
                 selectedSettingIndex == 0);

  const auto& settings = *currentSettings;
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.buttonHintsHeight +
                         metrics.verticalSpacing * 2)},
      settingsCount, selectedSettingIndex - 1,
      [&settings](int index) { return std::string(I18N.get(settings[index].nameId)); }, nullptr, nullptr,
      [&settings](int i) {
        const auto& setting = settings[i];
        std::string valueText = "";
        if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
          const bool value = SETTINGS.*(setting.valuePtr);
          valueText = value ? tr(STR_STATE_ON) : tr(STR_STATE_OFF);
        } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
          uint8_t value = SETTINGS.*(setting.valuePtr);
#if CROSSPOINT_PAPERS3
          if (setting.valuePtr == &CrossPointSettings::orientation) {
            value = CrossPointSettings::normalizePaperS3Orientation(value);
          }
#endif
          valueText = I18N.get(setting.enumValues[value]);
        } else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
          if (setting.nameId == StrId::STR_LINE_SPACING) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2fx", static_cast<float>(SETTINGS.*(setting.valuePtr)) / 100.0f);
            valueText = buf;
          } else {
            valueText = std::to_string(SETTINGS.*(setting.valuePtr));
          }
        } else if (setting.type == SettingType::ACTION && setting.nameId == StrId::STR_EXTERNAL_FONT) {
          const int selectedExternal = FontMgr.getSelectedIndex();
          if (selectedExternal >= 0) {
            const FontInfo* info = FontMgr.getFontInfo(selectedExternal);
            valueText = info ? buildExternalFontLabel(info->filename, info->name, info->size,
                                                      ExternalFont::canFitGlyph(info->width, info->height))
                             : tr(STR_EXTERNAL_FONT);
          } else {
            valueText = tr(STR_BUILTIN_FONT);
          }
        }
        return valueText;
      },
      true);

  // Draw help text
  const auto confirmLabel = (selectedSettingIndex == 0)
                                ? I18N.get(categoryNames[(selectedCategoryIndex + 1) % categoryCount])
                                : tr(STR_TOGGLE);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  // Always use standard refresh for settings screen
  renderer.displayBuffer();
}

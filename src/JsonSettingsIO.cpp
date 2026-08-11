#include "JsonSettingsIO.h"

#include <ArduinoJson.h>
#include <CredentialIntegrity.h>
#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "KOReaderCredentialStore.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "SettingsList.h"
#include "WifiCredentialStore.h"

namespace {
constexpr size_t KOREADER_PASSWORD_MAX_LENGTH = 64;
constexpr size_t OPDS_PASSWORD_MAX_LENGTH = 63;
constexpr size_t WIFI_PASSWORD_MAX_LENGTH = 64;

uint32_t passwordCrc32(const std::string& password) {
  return credential_integrity::crc32(std::string_view(password.data(), password.size()));
}
}  // namespace

// Convert legacy settings.
void applyLegacyStatusBarSettings(CrossPointSettings& settings) {
  switch (static_cast<CrossPointSettings::STATUS_BAR_MODE>(settings.statusBar)) {
    case CrossPointSettings::NONE:
      settings.statusBarChapterPageCount = 0;
      settings.statusBarBookProgressPercentage = 0;
      settings.statusBarProgressBar = CrossPointSettings::HIDE_PROGRESS;
      settings.statusBarTitle = CrossPointSettings::HIDE_TITLE;
      settings.statusBarBattery = 0;
      break;
    case CrossPointSettings::NO_PROGRESS:
      settings.statusBarChapterPageCount = 0;
      settings.statusBarBookProgressPercentage = 0;
      settings.statusBarProgressBar = CrossPointSettings::HIDE_PROGRESS;
      settings.statusBarTitle = CrossPointSettings::CHAPTER_TITLE;
      settings.statusBarBattery = 1;
      break;
    case CrossPointSettings::BOOK_PROGRESS_BAR:
      settings.statusBarChapterPageCount = 1;
      settings.statusBarBookProgressPercentage = 0;
      settings.statusBarProgressBar = CrossPointSettings::BOOK_PROGRESS;
      settings.statusBarTitle = CrossPointSettings::CHAPTER_TITLE;
      settings.statusBarBattery = 1;
      break;
    case CrossPointSettings::ONLY_BOOK_PROGRESS_BAR:
      settings.statusBarChapterPageCount = 1;
      settings.statusBarBookProgressPercentage = 0;
      settings.statusBarProgressBar = CrossPointSettings::BOOK_PROGRESS;
      settings.statusBarTitle = CrossPointSettings::HIDE_TITLE;
      settings.statusBarBattery = 0;
      break;
    case CrossPointSettings::CHAPTER_PROGRESS_BAR:
      settings.statusBarChapterPageCount = 0;
      settings.statusBarBookProgressPercentage = 1;
      settings.statusBarProgressBar = CrossPointSettings::CHAPTER_PROGRESS;
      settings.statusBarTitle = CrossPointSettings::CHAPTER_TITLE;
      settings.statusBarBattery = 1;
      break;
    case CrossPointSettings::FULL:
    default:
      settings.statusBarChapterPageCount = 1;
      settings.statusBarBookProgressPercentage = 1;
      settings.statusBarProgressBar = CrossPointSettings::HIDE_PROGRESS;
      settings.statusBarTitle = CrossPointSettings::CHAPTER_TITLE;
      settings.statusBarBattery = 1;
      break;
  }
}

// ---- CrossPointState ----

bool JsonSettingsIO::saveState(const CrossPointState& s, const char* path) {
  JsonDocument doc;
  doc["openEpubPath"] = s.openEpubPath;
  JsonArray recentArr = doc["recentSleepImages"].to<JsonArray>();
  for (int i = 0; i < CrossPointState::SLEEP_RECENT_COUNT; i++) recentArr.add(s.recentSleepImages[i]);
  doc["recentSleepPos"] = s.recentSleepPos;
  doc["recentSleepFill"] = s.recentSleepFill;
  doc["readerActivityLoadCount"] = s.readerActivityLoadCount;
  doc["lastSleepFromReader"] = s.lastSleepFromReader;

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(path, json);
}

bool JsonSettingsIO::loadState(CrossPointState& s, const char* json) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("CPS", "JSON parse error: %s", error.c_str());
    return false;
  }

  s.openEpubPath = doc["openEpubPath"] | std::string("");
  memset(s.recentSleepImages, 0, sizeof(s.recentSleepImages));
  JsonArrayConst recentArr = doc["recentSleepImages"];
  const int actualCount = recentArr.isNull() ? 0
                                             : std::min(static_cast<int>(recentArr.size()),
                                                        static_cast<int>(CrossPointState::SLEEP_RECENT_COUNT));
  for (int i = 0; i < actualCount; i++) s.recentSleepImages[i] = recentArr[i] | static_cast<uint16_t>(0);
  s.recentSleepPos = doc["recentSleepPos"] | static_cast<uint8_t>(0);
  if (s.recentSleepPos >= CrossPointState::SLEEP_RECENT_COUNT)
    s.recentSleepPos = actualCount > 0 ? s.recentSleepPos % CrossPointState::SLEEP_RECENT_COUNT : 0;
  s.recentSleepFill = doc["recentSleepFill"] | static_cast<uint8_t>(0);
  s.recentSleepFill = static_cast<uint8_t>(std::min(static_cast<int>(s.recentSleepFill), actualCount));
  // Migrate legacy single-image field from old state.json (pre-recency-buffer).
  // Only seeds the buffer if the new buffer is empty (fresh migration, not a resave).
  if (s.recentSleepFill == 0 && !doc["lastSleepImage"].isNull()) {
    const uint8_t legacy = doc["lastSleepImage"] | static_cast<uint8_t>(UINT8_MAX);
    if (legacy != UINT8_MAX) s.pushRecentSleep(static_cast<uint16_t>(legacy));
  }
  s.readerActivityLoadCount = doc["readerActivityLoadCount"] | static_cast<uint8_t>(0);
  s.lastSleepFromReader = doc["lastSleepFromReader"] | false;
  return true;
}

// ---- CrossPointSettings ----

bool JsonSettingsIO::saveSettings(const CrossPointSettings& s, const char* path) {
  JsonDocument doc;

  for (const auto& info : getSettingsList()) {
    if (!info.key) continue;
    // Dynamic entries (KOReader etc.) are stored in their own files — skip.
    if (!info.valuePtr && !info.stringOffset) continue;

    if (info.stringOffset) {
      const char* strPtr = (const char*)&s + info.stringOffset;
      if (info.obfuscated) {
        doc[std::string(info.key) + "_obf"] = obfuscation::obfuscateToBase64(strPtr);
      } else {
        doc[info.key] = strPtr;
      }
    } else {
      doc[info.key] = s.*(info.valuePtr);
    }
  }

  // Front button remap — managed by RemapFrontButtons sub-activity, not in SettingsList.
  doc["frontButtonBack"] = s.frontButtonBack;
  doc["frontButtonConfirm"] = s.frontButtonConfirm;
  doc["frontButtonLeft"] = s.frontButtonLeft;
  doc["frontButtonRight"] = s.frontButtonRight;

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(path, json);
}

bool JsonSettingsIO::loadSettings(CrossPointSettings& s, const char* json, bool* needsResave) {
  if (needsResave) *needsResave = false;
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("CPS", "JSON parse error: %s", error.c_str());
    return false;
  }

  auto clamp = [](uint8_t val, uint8_t maxVal, uint8_t def) -> uint8_t { return val < maxVal ? val : def; };

  // Legacy migration: if statusBarChapterPageCount is absent this is a pre-refactor settings file.
  // Populate s with migrated values now so the generic loop below picks them up as defaults and clamps them.
  if (doc["statusBarChapterPageCount"].isNull()) {
    applyLegacyStatusBarSettings(s);
  }

  for (const auto& info : getSettingsList()) {
    if (!info.key) continue;
    // Dynamic entries (KOReader etc.) are stored in their own files — skip.
    if (!info.valuePtr && !info.stringOffset) continue;

    if (info.stringOffset) {
      const char* strPtr = (const char*)&s + info.stringOffset;
      const std::string fieldDefault = strPtr;  // current buffer = struct-initializer default
      std::string val;
      if (info.obfuscated) {
        bool ok = false;
        bool tooLong = false;
        const size_t maxDecodedLength = info.stringMaxLen > 0 ? info.stringMaxLen - 1 : 0;
        val = obfuscation::deobfuscateFromBase64(doc[std::string(info.key) + "_obf"] | "", maxDecodedLength, &ok,
                                                 &tooLong);
        if (tooLong) {
          LOG_ERR("CPS", "Oversized obfuscated value for key '%s'", info.key);
          val = fieldDefault;
          if (needsResave) *needsResave = true;
        } else if (!ok || val.empty()) {
          val = doc[info.key] | fieldDefault;
          if (val != fieldDefault && needsResave) *needsResave = true;
        }
      } else {
        val = doc[info.key] | fieldDefault;
      }
      char* destPtr = (char*)&s + info.stringOffset;
      if (info.stringMaxLen == 0) {
        LOG_ERR("CPS", "Misconfigured SettingInfo: stringMaxLen is 0 for key '%s'", info.key);
        destPtr[0] = '\0';
        if (needsResave) *needsResave = true;
        continue;
      }
      strncpy(destPtr, val.c_str(), info.stringMaxLen - 1);
      destPtr[info.stringMaxLen - 1] = '\0';
    } else {
      const uint8_t fieldDefault = s.*(info.valuePtr);  // struct-initializer default, read before we overwrite it
      uint8_t v = doc[info.key] | fieldDefault;
      if (info.type == SettingType::ENUM) {
        v = clamp(v, (uint8_t)info.enumValues.size(), fieldDefault);
      } else if (info.type == SettingType::TOGGLE) {
        v = clamp(v, (uint8_t)2, fieldDefault);
      } else if (info.type == SettingType::VALUE) {
        if (v < info.valueRange.min)
          v = info.valueRange.min;
        else if (v > info.valueRange.max)
          v = info.valueRange.max;
      }
      s.*(info.valuePtr) = v;
    }
  }

  // Legacy OPDS single-server settings were moved to OpdsServerStore. Keep
  // reading these keys so OpdsServerStore can migrate older settings.json files.
  const std::string legacyOpdsUrl = doc["opdsServerUrl"] | std::string("");
  if (!legacyOpdsUrl.empty()) {
    strncpy(s.opdsServerUrl, legacyOpdsUrl.c_str(), sizeof(s.opdsServerUrl) - 1);
    s.opdsServerUrl[sizeof(s.opdsServerUrl) - 1] = '\0';
    strncpy(s.opdsUsername, (doc["opdsUsername"] | std::string("")).c_str(), sizeof(s.opdsUsername) - 1);
    s.opdsUsername[sizeof(s.opdsUsername) - 1] = '\0';

    bool ok = false;
    bool tooLong = false;
    std::string password =
        obfuscation::deobfuscateFromBase64(doc["opdsPassword_obf"] | "", sizeof(s.opdsPassword) - 1, &ok, &tooLong);
    if (tooLong) {
      LOG_ERR("CPS", "Oversized legacy OPDS password");
      password.clear();
    } else if (!ok || password.empty()) {
      const char* legacyPassword = doc["opdsPassword"] | "";
      if (strlen(legacyPassword) < sizeof(s.opdsPassword)) {
        password = legacyPassword;
      } else {
        LOG_ERR("CPS", "Oversized legacy OPDS plaintext password");
        password.clear();
      }
    }
    strncpy(s.opdsPassword, password.c_str(), sizeof(s.opdsPassword) - 1);
    s.opdsPassword[sizeof(s.opdsPassword) - 1] = '\0';
  }

  // Front button remap — managed by RemapFrontButtons sub-activity, not in SettingsList.
  using S = CrossPointSettings;
  s.frontButtonBack =
      clamp(doc["frontButtonBack"] | (uint8_t)S::FRONT_HW_BACK, S::FRONT_BUTTON_HARDWARE_COUNT, S::FRONT_HW_BACK);
  s.frontButtonConfirm = clamp(doc["frontButtonConfirm"] | (uint8_t)S::FRONT_HW_CONFIRM, S::FRONT_BUTTON_HARDWARE_COUNT,
                               S::FRONT_HW_CONFIRM);
  s.frontButtonLeft =
      clamp(doc["frontButtonLeft"] | (uint8_t)S::FRONT_HW_LEFT, S::FRONT_BUTTON_HARDWARE_COUNT, S::FRONT_HW_LEFT);
  s.frontButtonRight =
      clamp(doc["frontButtonRight"] | (uint8_t)S::FRONT_HW_RIGHT, S::FRONT_BUTTON_HARDWARE_COUNT, S::FRONT_HW_RIGHT);
  CrossPointSettings::validateFrontButtonMapping(s);

  // Legacy lineSpacing migration: old enum values (TIGHT=0, NORMAL=1, WIDE=2)
  // and legacy slider values (20..60) must be converted to the percent-based format.
  {
    const uint8_t rawLineSpacing = doc["lineSpacing"] | (uint8_t)S::LINE_SPACING_DEFAULT;
    if (rawLineSpacing < S::LINE_COMPRESSION_COUNT) {
      if (needsResave) *needsResave = true;
      switch (rawLineSpacing) {
        case S::TIGHT:
          s.lineSpacing = 90;
          break;
        case S::WIDE:
          s.lineSpacing = 120;
          break;
        case S::NORMAL:
        default:
          s.lineSpacing = S::LINE_SPACING_DEFAULT;
          break;
      }
    } else if (rawLineSpacing >= 20 && rawLineSpacing <= 60) {
      if (needsResave) *needsResave = true;
      s.lineSpacing = S::LINE_SPACING_DEFAULT;
    }
  }

  if (doc["firstLineIndent"].isNull() && s.extraParagraphSpacing == 0) {
    // Preserve pre-setting behavior: disabling extra paragraph spacing used to imply
    // a fallback first-line indent for EPUB paragraphs without CSS text-indent.
    s.firstLineIndent = 1;
    if (needsResave) *needsResave = true;
  }

  LOG_DBG("CPS", "Settings loaded from file");

  return true;
}

// ---- KOReaderCredentialStore ----

bool JsonSettingsIO::saveKOReader(const KOReaderCredentialStore& store, const char* path) {
  JsonDocument doc;
  doc["username"] = store.getUsername();
  doc["password_obf"] = obfuscation::obfuscateToBase64(store.getPassword());
  doc["serverUrl"] = store.getServerUrl();
  doc["matchMethod"] = static_cast<uint8_t>(store.getMatchMethod());

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(path, json);
}

bool JsonSettingsIO::loadKOReader(KOReaderCredentialStore& store, const char* json, bool* needsResave) {
  if (needsResave) *needsResave = false;
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("KRS", "JSON parse error: %s", error.c_str());
    return false;
  }

  store.username = doc["username"] | std::string("");
  bool ok = false;
  bool tooLong = false;
  store.password =
      obfuscation::deobfuscateFromBase64(doc["password_obf"] | "", KOREADER_PASSWORD_MAX_LENGTH, &ok, &tooLong);
  if (tooLong) {
    LOG_ERR("KRS", "Oversized KOReader password");
    store.password.clear();
    if (needsResave) *needsResave = true;
  } else if (!ok || store.password.empty()) {
    const char* legacyPassword = doc["password"] | "";
    if (strlen(legacyPassword) <= KOREADER_PASSWORD_MAX_LENGTH) {
      store.password = legacyPassword;
    } else {
      LOG_ERR("KRS", "Oversized KOReader plaintext password");
      store.password.clear();
    }
    if (!store.password.empty() && needsResave) *needsResave = true;
  }
  store.serverUrl = doc["serverUrl"] | std::string("");
  uint8_t method = doc["matchMethod"] | (uint8_t)0;
  store.matchMethod = static_cast<DocumentMatchMethod>(method);

  LOG_DBG("KRS", "Loaded KOReader credentials for user: %s", store.username.c_str());
  return true;
}

// ---- OpdsServerStore ----

bool JsonSettingsIO::saveOpds(const OpdsServerStore& store, const char* path) {
  JsonDocument doc;

  JsonArray arr = doc["servers"].to<JsonArray>();
  for (const auto& server : store.getServers()) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = server.name;
    obj["url"] = server.url;
    obj["username"] = server.username;
    obj["password_obf"] = obfuscation::obfuscateToBase64(server.password);
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(path, json);
}

bool JsonSettingsIO::loadOpds(OpdsServerStore& store, const char* json, bool* needsResave) {
  if (needsResave) *needsResave = false;

  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("OPS", "JSON parse error: %s", error.c_str());
    return false;
  }

  store.servers.clear();
  store.servers.reserve(OpdsServerStore::MAX_SERVERS);

  JsonArray arr = doc["servers"].as<JsonArray>();
  for (JsonObject obj : arr) {
    if (store.servers.size() >= OpdsServerStore::MAX_SERVERS) {
      break;
    }

    OpdsServer server;
    server.name = obj["name"] | std::string("");
    server.url = obj["url"] | std::string("");
    server.username = obj["username"] | std::string("");

    bool ok = false;
    bool tooLong = false;
    server.password =
        obfuscation::deobfuscateFromBase64(obj["password_obf"] | "", OPDS_PASSWORD_MAX_LENGTH, &ok, &tooLong);
    if (tooLong) {
      LOG_ERR("OPS", "Oversized OPDS password for %s", server.name.c_str());
      server.password.clear();
      if (needsResave) {
        *needsResave = true;
      }
    } else if (!ok || server.password.empty()) {
      const char* legacyPassword = obj["password"] | "";
      if (strlen(legacyPassword) <= OPDS_PASSWORD_MAX_LENGTH) {
        server.password = legacyPassword;
      } else {
        LOG_ERR("OPS", "Oversized OPDS plaintext password for %s", server.name.c_str());
        server.password.clear();
      }
      if (!server.password.empty() && needsResave) {
        *needsResave = true;
      }
    }

    store.servers.push_back(std::move(server));
  }

  LOG_DBG("OPS", "Loaded %zu OPDS servers from file", store.servers.size());
  return true;
}

// ---- WifiCredentialStore ----

bool JsonSettingsIO::saveWifi(const WifiCredentialStore& store, const char* path) {
  JsonDocument doc;
  doc["lastConnectedSsid"] = store.getLastConnectedSsid();

  JsonArray arr = doc["credentials"].to<JsonArray>();
  const auto credentials = store.getCredentialsSnapshot();
  for (const auto& cred : credentials) {
    JsonObject obj = arr.add<JsonObject>();
    obj["ssid"] = cred.ssid;
    obj["password_obf"] = obfuscation::obfuscateToBase64(cred.password);
    obj["password_len"] = cred.password.size();
    obj["password_crc32"] = passwordCrc32(cred.password);
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(path, json);
}

bool JsonSettingsIO::loadWifi(WifiCredentialStore& store, const char* json, bool* needsResave) {
  if (needsResave) *needsResave = false;
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("WCS", "JSON parse error: %s", error.c_str());
    return false;
  }

  std::string loadedLastSsid = doc["lastConnectedSsid"] | std::string("");

  std::vector<WifiCredential> loadedCredentials;
  loadedCredentials.reserve(WifiCredentialStore::MAX_NETWORKS);
  JsonArray arr = doc["credentials"].as<JsonArray>();
  for (JsonObject obj : arr) {
    if (loadedCredentials.size() >= WifiCredentialStore::MAX_NETWORKS) break;
    WifiCredential cred;
    cred.ssid = obj["ssid"] | std::string("");
    const JsonVariantConst passwordLength = obj["password_len"];
    const bool hasPasswordLength = !passwordLength.isNull();
    size_t expectedLength = 0;
    if (hasPasswordLength) {
      if (!passwordLength.is<size_t>()) {
        LOG_ERR("WCS", "Discarding corrupted password for %s (invalid length)", cred.ssid.c_str());
        if (needsResave) *needsResave = true;
        continue;
      }
      expectedLength = passwordLength.as<size_t>();
      if (expectedLength > WIFI_PASSWORD_MAX_LENGTH) {
        LOG_ERR("WCS", "Discarding oversized password for %s (%zu bytes)", cred.ssid.c_str(), expectedLength);
        if (needsResave) *needsResave = true;
        continue;
      }
    }

    bool ok = false;
    bool tooLong = false;
    cred.password =
        obfuscation::deobfuscateFromBase64(obj["password_obf"] | "", WIFI_PASSWORD_MAX_LENGTH, &ok, &tooLong);
    if (tooLong) {
      LOG_ERR("WCS", "Discarding oversized password for %s", cred.ssid.c_str());
      if (needsResave) *needsResave = true;
      continue;
    }
    if (!ok || cred.password.empty()) {
      const char* legacyPassword = obj["password"] | "";
      if (strlen(legacyPassword) > WIFI_PASSWORD_MAX_LENGTH) {
        LOG_ERR("WCS", "Discarding oversized plaintext password for %s", cred.ssid.c_str());
        if (needsResave) *needsResave = true;
        continue;
      }
      cred.password = legacyPassword;
      if (!cred.password.empty() && needsResave) *needsResave = true;
    }
    if (hasPasswordLength) {
      if (cred.password.size() != expectedLength) {
        LOG_ERR("WCS", "Discarding corrupted password for %s (expected %zu bytes, decoded %zu)", cred.ssid.c_str(),
                expectedLength, cred.password.size());
        if (needsResave) *needsResave = true;
        continue;
      }
    } else {
      if (needsResave) *needsResave = true;
    }

    const JsonVariantConst checksum = obj["password_crc32"];
    if (checksum.is<uint32_t>()) {
      const uint32_t expectedCrc32 = checksum.as<uint32_t>();
      if (!credential_integrity::validate(std::string_view(cred.password.data(), cred.password.size()),
                                          cred.password.size(), expectedCrc32)) {
        LOG_ERR("WCS", "Discarding corrupted password for %s (checksum mismatch)", cred.ssid.c_str());
        if (needsResave) *needsResave = true;
        continue;
      }
    } else if (checksum.isNull()) {
      if (needsResave) *needsResave = true;
    } else {
      LOG_ERR("WCS", "Discarding corrupted password for %s (invalid checksum)", cred.ssid.c_str());
      if (needsResave) *needsResave = true;
      continue;
    }
    loadedCredentials.push_back(std::move(cred));
  }

  store.replaceLoadedCredentials(std::move(loadedLastSsid), std::move(loadedCredentials));
  LOG_DBG("WCS", "Loaded %zu WiFi credentials from file", store.getCredentialCount());
  return true;
}

// ---- RecentBooksStore ----

bool JsonSettingsIO::saveRecentBooks(const RecentBooksStore& store, const char* path) {
  JsonDocument doc;
  JsonArray arr = doc["books"].to<JsonArray>();
  for (const auto& book : store.getBooks()) {
    JsonObject obj = arr.add<JsonObject>();
    obj["path"] = book.path;
    obj["title"] = book.title;
    obj["author"] = book.author;
    obj["coverBmpPath"] = book.coverBmpPath;
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(path, json);
}

bool JsonSettingsIO::loadRecentBooks(RecentBooksStore& store, const char* json) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("RBS", "JSON parse error: %s", error.c_str());
    return false;
  }

  store.recentBooks.clear();
  JsonArray arr = doc["books"].as<JsonArray>();
  for (JsonObject obj : arr) {
    if (store.getCount() >= 10) break;
    RecentBook book;
    book.path = obj["path"] | std::string("");
    book.title = obj["title"] | std::string("");
    book.author = obj["author"] | std::string("");
    book.coverBmpPath = obj["coverBmpPath"] | std::string("");
    store.recentBooks.push_back(book);
  }

  LOG_DBG("RBS", "Recent books loaded from file (%d entries)", store.getCount());
  return true;
}

#include "OpdsServerStore.h"

#include <HalStorage.h>
#include <JsonSettingsIO.h>
#include <Logging.h>

#include <cstring>
#include <utility>

#include "CrossPointSettings.h"

OpdsServerStore OpdsServerStore::instance;

namespace {
constexpr char OPDS_FILE_JSON[] = "/.crosspoint/opds.json";
}

OpdsServerStore::OpdsServerStore() { servers.reserve(MAX_SERVERS); }

bool OpdsServerStore::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  return JsonSettingsIO::saveOpds(*this, OPDS_FILE_JSON);
}

bool OpdsServerStore::loadFromFile() {
  servers.reserve(MAX_SERVERS);

  if (Storage.exists(OPDS_FILE_JSON)) {
    String json = Storage.readFile(OPDS_FILE_JSON);
    if (!json.isEmpty()) {
      bool resave = false;
      const bool result = JsonSettingsIO::loadOpds(*this, json.c_str(), &resave);
      if (result && resave) {
        LOG_DBG("OPS", "Resaving OPDS JSON with obfuscated passwords");
        saveToFile();
      }
      return result;
    }
  }

  if (migrateFromSettings()) {
    LOG_DBG("OPS", "Migrated legacy OPDS settings");
    return true;
  }

  return false;
}

bool OpdsServerStore::migrateFromSettings() {
  if (strlen(SETTINGS.opdsServerUrl) == 0) {
    return false;
  }

  servers.clear();
  servers.reserve(MAX_SERVERS);

  OpdsServer server;
  server.name = "OPDS Server";
  server.url = SETTINGS.opdsServerUrl;
  server.username = SETTINGS.opdsUsername;
  server.password = SETTINGS.opdsPassword;
  servers.push_back(std::move(server));

  if (!saveToFile()) {
    servers.clear();
    return false;
  }

  SETTINGS.opdsServerUrl[0] = '\0';
  SETTINGS.opdsUsername[0] = '\0';
  SETTINGS.opdsPassword[0] = '\0';
  SETTINGS.saveToFile();
  return true;
}

bool OpdsServerStore::addServer(const OpdsServer& server) {
  if (servers.size() >= MAX_SERVERS) {
    LOG_ERR("OPS", "Cannot add more OPDS servers, limit of %zu reached", MAX_SERVERS);
    return false;
  }

  servers.push_back(server);
  return saveToFile();
}

bool OpdsServerStore::updateServer(const size_t index, const OpdsServer& server) {
  if (index >= servers.size()) {
    return false;
  }

  servers[index] = server;
  return saveToFile();
}

bool OpdsServerStore::removeServer(const size_t index) {
  if (index >= servers.size()) {
    return false;
  }

  servers.erase(servers.begin() + static_cast<ptrdiff_t>(index));
  return saveToFile();
}

const OpdsServer* OpdsServerStore::getServer(const size_t index) const {
  if (index >= servers.size()) {
    return nullptr;
  }
  return &servers[index];
}

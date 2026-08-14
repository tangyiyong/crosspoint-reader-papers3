#include "WifiUtils.h"

#include <WiFi.h>

#include <algorithm>
#include <vector>

#include "WifiCredentialStore.h"

namespace {
WifiUtils::ConnectionStatus connectionStatus;

void setConnectionState(const WifiUtils::ConnectionState state, const std::string& ssid = std::string()) {
  connectionStatus.state = state;
  connectionStatus.ssid = ssid;
  connectionStatus.rssi = 0;
  connectionStatus.updatedAt = millis();
}

void refreshConnectionStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    const String wifiSsid = WiFi.SSID();
    connectionStatus.state = WifiUtils::ConnectionState::Connected;
    if (wifiSsid.length() > 0) {
      connectionStatus.ssid = wifiSsid.c_str();
    }
    connectionStatus.rssi = WiFi.RSSI();
    connectionStatus.updatedAt = millis();
    return;
  }

  if (connectionStatus.state != WifiUtils::ConnectionState::Connecting) {
    connectionStatus.state = WifiUtils::ConnectionState::Disconnected;
    connectionStatus.rssi = 0;
    connectionStatus.updatedAt = millis();
  }
}

void startConnectionAttempt(const WifiCredential& cred) {
  setConnectionState(WifiUtils::ConnectionState::Connecting, cred.ssid);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
  if (cred.password.empty()) {
    WiFi.begin(cred.ssid.c_str());
  } else {
    WiFi.begin(cred.ssid.c_str(), cred.password.c_str());
  }
}
}  // namespace

namespace WifiUtils {
bool isConnected() {
  refreshConnectionStatus();
  return connectionStatus.state == ConnectionState::Connected;
}

ConnectionStatus getConnectionStatus() {
  refreshConnectionStatus();
  return connectionStatus;
}

bool waitForConnected(const unsigned long timeoutMs) {
  const unsigned long startedAt = millis();
  while (!isConnected() && millis() - startedAt < timeoutMs) {
    delay(100);
  }
  return isConnected();
}

bool ensureConnected(const unsigned long perNetworkTimeoutMs) {
  if (isConnected()) {
    return true;
  }

  std::vector<WifiCredential> credentials = WIFI_STORE.getCredentialsSnapshot();
  if (credentials.empty()) {
    setConnectionState(ConnectionState::Disconnected);
    return false;
  }

  const std::string lastSsid = WIFI_STORE.getLastConnectedSsid();
  if (!lastSsid.empty()) {
    auto lastIt = std::find_if(credentials.begin(), credentials.end(),
                               [&lastSsid](const WifiCredential& cred) { return cred.ssid == lastSsid; });
    if (lastIt != credentials.end() && lastIt != credentials.begin()) {
      std::rotate(credentials.begin(), lastIt, lastIt + 1);
    }
  }

  for (const auto& cred : credentials) {
    if (cred.ssid.empty()) {
      continue;
    }
    startConnectionAttempt(cred);
    if (waitForConnected(perNetworkTimeoutMs)) {
      const std::string connectedSsid = connectionStatus.ssid.empty() ? cred.ssid : connectionStatus.ssid;
      connectionStatus.ssid = connectedSsid;
      WIFI_STORE.setLastConnectedSsid(connectedSsid);
      return true;
    }
  }

  WiFi.disconnect();
  setConnectionState(ConnectionState::Failed);
  return false;
}

bool ensureConnectedFromSavedCredential(const unsigned long timeoutMs) {
  return ensureConnected(timeoutMs);
}
}  // namespace WifiUtils

#pragma once

#include <cstdint>
#include <string>

namespace WifiUtils {
enum class ConnectionState : uint8_t { Disconnected, Connecting, Connected, Failed };

struct ConnectionStatus {
  ConnectionState state = ConnectionState::Disconnected;
  std::string ssid;
  int32_t rssi = 0;
  unsigned long updatedAt = 0;
};

bool isConnected();
ConnectionStatus getConnectionStatus();
bool waitForConnected(unsigned long timeoutMs = 5000);
bool ensureConnected(unsigned long perNetworkTimeoutMs = 8000);
bool ensureConnectedFromSavedCredential(unsigned long timeoutMs = 8000);
}  // namespace WifiUtils

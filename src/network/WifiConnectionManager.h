#ifndef WIFI_CONNECTION_MANAGER_H
#define WIFI_CONNECTION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

#include "../storage/AppPreferences.h"

class WifiConnectionManager {
 public:
  WifiConnectionManager();

  void begin(AppPreferences* appPrefs);
  void connectAtBoot();
  void setEnabled(bool enabled);
  void setApModeActive(bool active);
  void tick();

  bool isConnected() const;
  bool ensureConnected(uint32_t timeoutMs = 12000);

 private:
  void reloadCredentials();
  void startConnect();
  WiFiMode_t desiredMode() const;

  AppPreferences* appPrefs_;
  WifiCredentials creds_;
  bool enabled_;
  bool apModeActive_;
  bool connecting_;
  unsigned long lastCheckMs_;
  unsigned long connectStartedMs_;
  unsigned long nextReconnectMs_;

  static const uint32_t kCheckIntervalMs = 5000;
  static const uint32_t kBootConnectTimeoutMs = 12000;
  static const uint32_t kReconnectBackoffMs = 10000;
};

#endif

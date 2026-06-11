#include "PingSender.h"

#include <WiFi.h>

#include "PosLink.h"

PingSender::PingSender()
    : appPrefs_(nullptr), fwVersion_(""), lastPingMs_(0), lastIntervalCheckMs_(0), intervalSec_(0) {}

void PingSender::begin(AppPreferences* appPrefs, const char* fwVersion) {
  appPrefs_ = appPrefs;
  fwVersion_ = fwVersion != nullptr ? fwVersion : "";
  intervalSec_ = appPrefs_->loadPingIntervalSec();
  lastPingMs_ = millis();
}

void PingSender::tick(PosLink& link, bool measurementActive) {
  if (appPrefs_ == nullptr || measurementActive || !link.isConnected()) {
    return;
  }

  const unsigned long now = millis();

  // Interwal z NVS odswiezany najwyzej raz na 5 s (portal mogl go zmienic) —
  // odczyt Preferences w kazdej iteracji petli to zbedny narzut.
  if (now - lastIntervalCheckMs_ >= 5000) {
    lastIntervalCheckMs_ = now;
    intervalSec_ = appPrefs_->loadPingIntervalSec();
  }
  if (intervalSec_ == 0) {
    return;
  }

  const unsigned long intervalMs = static_cast<unsigned long>(intervalSec_) * 1000UL;
  if (now - lastPingMs_ < intervalMs) {
    return;
  }
  lastPingMs_ = now;

  const String mac = WiFi.macAddress();
  const String ip = WiFi.localIP().toString();
  const String ssid = WiFi.SSID();
  link.sendPing(mac.c_str(), ip.c_str(), ssid.c_str(), WiFi.RSSI(), millis() / 1000UL,
                ESP.getFreeHeap());
  Serial.println("[ws] ping wyslany");
}

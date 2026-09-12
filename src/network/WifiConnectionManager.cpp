#include "WifiConnectionManager.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetBIOS.h>

// Nazwa widoczna w sieci (DHCP hostname, mDNS wagawezy.local, NetBIOS) —
// skanery LAN pokazuja ja zamiast samego adresu IP.
static const char* kNetName = "WagaWezy";

WifiConnectionManager::WifiConnectionManager()
    : appPrefs_(nullptr),
      creds_(),
      enabled_(true),
      apModeActive_(false),
      connecting_(false),
      nameAnnounced_(false),
      lastCheckMs_(0),
      connectStartedMs_(0),
      nextReconnectMs_(0) {}

void WifiConnectionManager::begin(AppPreferences* appPrefs) {
  appPrefs_ = appPrefs;
  reloadCredentials();
}

void WifiConnectionManager::reloadCredentials() {
  if (appPrefs_ == nullptr) {
    creds_.valid = false;
    return;
  }
  creds_ = appPrefs_->loadWifiCredentials();
}

WiFiMode_t WifiConnectionManager::desiredMode() const {
  return apModeActive_ ? WIFI_AP_STA : WIFI_STA;
}

// mDNS i NetBIOS startuja raz po pierwszym polaczeniu; obie uslugi przezywaja
// reconnect WiFi (nasluch UDP na wszystkich interfejsach).
void WifiConnectionManager::announceName() {
  if (nameAnnounced_) {
    return;
  }
  nameAnnounced_ = true;
  if (MDNS.begin("wagawezy")) {
    MDNS.setInstanceName(kNetName);
  } else {
    Serial.println(F("WiFi: mDNS start nieudany"));
  }
  if (!NBNS.begin(kNetName)) {
    Serial.println(F("WiFi: NetBIOS start nieudany"));
  }
}

void WifiConnectionManager::startConnect() {
  if (!enabled_ || !creds_.valid) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == creds_.ssid) {
    connecting_ = false;
    return;
  }

  WiFi.setHostname(kNetName);
  WiFi.mode(desiredMode());
  // Waga jest zasilana z sieci — wyłączamy modem sleep: stabilniejsza sesja
  // przy bezczynności (router rzadziej zrywa) i niższa latencja.
  WiFi.setSleep(false);
  WiFi.begin(creds_.ssid.c_str(), creds_.password.c_str());
  connecting_ = true;
  connectStartedMs_ = millis();
}

void WifiConnectionManager::connectAtBoot() {
  reloadCredentials();
  if (!creds_.valid) {
    Serial.println(F("WiFi: brak zapisanej sieci"));
    return;
  }

  Serial.print(F("WiFi: laczenie z "));
  Serial.println(creds_.ssid);
  startConnect();

  const unsigned long start = millis();
  while (connecting_ && (millis() - start) < kBootConnectTimeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      connecting_ = false;
      break;
    }
    delay(50);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("WiFi: polaczono, IP "));
    Serial.println(WiFi.localIP());
    announceName();
  } else {
    Serial.println(F("WiFi: timeout przy starcie, reconnect w tle"));
    connecting_ = false;
    nextReconnectMs_ = millis() + kReconnectBackoffMs;
  }
}

void WifiConnectionManager::setEnabled(bool enabled) {
  enabled_ = enabled;
  if (enabled) {
    reloadCredentials();
    nextReconnectMs_ = millis();
  } else {
    connecting_ = false;
  }
}

void WifiConnectionManager::setApModeActive(bool active) {
  apModeActive_ = active;
  if (active) {
    WiFi.mode(WIFI_AP_STA);
  } else if (enabled_ && creds_.valid) {
    WiFi.mode(WIFI_STA);
    if (WiFi.status() != WL_CONNECTED) {
      nextReconnectMs_ = millis();
    }
  }
}

bool WifiConnectionManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WifiConnectionManager::ensureConnected(uint32_t timeoutMs) {
  reloadCredentials();
  if (!creds_.valid) {
    return false;
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == creds_.ssid) {
    return true;
  }

  startConnect();
  const unsigned long start = millis();
  while ((millis() - start) < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      connecting_ = false;
      announceName();
      return true;
    }
    delay(10);
  }

  connecting_ = false;
  nextReconnectMs_ = millis() + kReconnectBackoffMs;
  return false;
}

void WifiConnectionManager::tick() {
  if (!enabled_) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastCheckMs_ < kCheckIntervalMs) {
    return;
  }
  lastCheckMs_ = now;

  reloadCredentials();
  if (!creds_.valid) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == creds_.ssid) {
    connecting_ = false;
    announceName();
    return;
  }

  if (connecting_) {
    if (now - connectStartedMs_ > kBootConnectTimeoutMs) {
      connecting_ = false;
      nextReconnectMs_ = now + kReconnectBackoffMs;
      Serial.println(F("WiFi: reconnect timeout"));
    }
    return;
  }

  if (now < nextReconnectMs_) {
    return;
  }

  Serial.println(F("WiFi: reconnect..."));
  startConnect();
}

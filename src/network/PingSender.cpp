#include "PingSender.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

PingSender::PingSender() : appPrefs_(nullptr), fwVersion_(""), lastPingMs_(0), intervalSec_(0) {}

void PingSender::begin(AppPreferences* appPrefs, const char* fwVersion) {
  appPrefs_ = appPrefs;
  fwVersion_ = fwVersion != nullptr ? fwVersion : "";
  intervalSec_ = appPrefs_->loadPingIntervalSec();
  lastPingMs_ = millis();
}

void PingSender::tick(bool wifiConnected, bool measurementActive) {
  if (appPrefs_ == nullptr || measurementActive || !wifiConnected) {
    return;
  }

  intervalSec_ = appPrefs_->loadPingIntervalSec();
  if (intervalSec_ == 0) {
    return;
  }

  const unsigned long intervalMs = static_cast<unsigned long>(intervalSec_) * 1000UL;
  const unsigned long now = millis();
  if (now - lastPingMs_ < intervalMs) {
    return;
  }

  const String endpoint = appPrefs_->loadApiEndpoint();
  if (!AppPreferences::isValidApiEndpoint(endpoint)) {
    return;
  }

  if (sendPing(endpoint)) {
    lastPingMs_ = now;
  }
}

String PingSender::buildPingBody() const {
  String body;
  body.reserve(176);
  body += "{\"type\":\"ping\",\"deviceId\":\"";
  body += WiFi.macAddress();
  body += "\",\"fw\":\"";
  body += fwVersion_;
  body += "\",\"rssi\":";
  body += WiFi.RSSI();
  body += ",\"uptimeSec\":";
  body += millis() / 1000UL;
  body += ",\"freeHeap\":";
  body += ESP.getFreeHeap();
  body += "}";
  return body;
}

bool PingSender::sendPing(const String& endpoint) {
  const bool useTls = endpoint.startsWith("https://");
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  bool began = false;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;

  if (useTls) {
    secureClient.setInsecure();
    began = http.begin(secureClient, endpoint);
  } else {
    began = http.begin(plainClient, endpoint);
  }

  if (!began) {
    Serial.println(F("Ping: HTTP begin failed"));
    return false;
  }

  const String pingBody = buildPingBody();
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "waga-wezy-esp32");

  Serial.println(F("--- Ping ---"));
  Serial.print(F("POST "));
  Serial.println(endpoint);
  Serial.println(pingBody);

  const int status = http.POST(pingBody);
  http.getString();
  http.end();

  if (status >= 200 && status < 300) {
    Serial.print(F("Ping OK HTTP "));
    Serial.println(status);
    return true;
  }

  Serial.print(F("Ping blad HTTP "));
  Serial.println(status);
  return false;
}

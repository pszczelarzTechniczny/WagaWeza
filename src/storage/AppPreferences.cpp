#include "AppPreferences.h"

const char* AppPreferences::kNamespace = "NETCFG";
const char* AppPreferences::kSsidKey = "wifi_ssid";
const char* AppPreferences::kPassKey = "wifi_pass";
const char* AppPreferences::kWelcomeLine1Key = "welcome_l1";
const char* AppPreferences::kWelcomeLine2Key = "welcome_l2";
const char* AppPreferences::kWelcomeLine3Key = "welcome_l3";
const char* AppPreferences::kApiEndpointKey = "api_endpoint";
const char* AppPreferences::kApPinKey = "ap_pin";
const char* AppPreferences::kPingIntervalKey = "ping_interval";

AppPreferences::AppPreferences() {}

bool AppPreferences::begin() {
  Preferences prefs;
  const bool opened = prefs.begin(kNamespace, false);
  if (opened) {
    prefs.end();
  }
  return opened;
}

WifiCredentials AppPreferences::loadWifiCredentials() const {
  WifiCredentials creds;
  creds.valid = false;

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return creds;
  }

  creds.ssid = prefs.getString(kSsidKey, "");
  creds.password = prefs.getString(kPassKey, "");
  prefs.end();

  creds.valid = creds.ssid.length() > 0;
  return creds;
}

bool AppPreferences::saveWifiCredentials(const String& ssid, const String& password) {
  if (ssid.length() == 0) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }

  const size_t ssidWritten = prefs.putString(kSsidKey, ssid);
  const size_t passWritten = prefs.putString(kPassKey, password);
  prefs.end();

  const bool ok1 = ssidWritten > 0;
  const bool ok2 = (password.length() == 0) ? (passWritten == 0) : (passWritten > 0);
  return ok1 && ok2;
}

bool AppPreferences::clearWifiCredentials() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }

  prefs.remove(kSsidKey);
  prefs.remove(kPassKey);
  prefs.end();
  return true;
}

String AppPreferences::trimWelcomeLine(const String& value) {
  String trimmed = value;
  trimmed.trim();
  if (trimmed.length() > kMaxWelcomeLineLen) {
    trimmed.remove(kMaxWelcomeLineLen);
  }
  return trimmed;
}

WelcomeMessage AppPreferences::defaultWelcomeMessage() {
  WelcomeMessage message;
  message.line1 = "Witam";
  message.line2 = "Pszczelarza";
  message.line3 = "z W\u0105chocka";
  return message;
}

WelcomeMessage AppPreferences::loadWelcomeMessage() const {
  const WelcomeMessage defaults = defaultWelcomeMessage();
  WelcomeMessage message = defaults;

  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return message;
  }

  const bool hasLine1 = prefs.isKey(kWelcomeLine1Key);
  const bool hasLine2 = prefs.isKey(kWelcomeLine2Key);
  const bool hasLine3 = prefs.isKey(kWelcomeLine3Key);

  if (hasLine1) {
    message.line1 = prefs.getString(kWelcomeLine1Key, defaults.line1.c_str());
  }
  if (hasLine2) {
    message.line2 = prefs.getString(kWelcomeLine2Key, defaults.line2.c_str());
  }
  if (hasLine3) {
    message.line3 = prefs.getString(kWelcomeLine3Key, defaults.line3.c_str());
  }
  prefs.end();

  if (!hasLine1 && !hasLine2 && !hasLine3) {
    return defaults;
  }
  return message;
}

bool AppPreferences::saveWelcomeMessage(const WelcomeMessage& message) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }

  prefs.putString(kWelcomeLine1Key, trimWelcomeLine(message.line1));
  prefs.putString(kWelcomeLine2Key, trimWelcomeLine(message.line2));
  prefs.putString(kWelcomeLine3Key, trimWelcomeLine(message.line3));
  prefs.end();
  return true;
}

String AppPreferences::trimEndpoint(const String& value) {
  String trimmed = value;
  trimmed.trim();
  if (trimmed.length() > kMaxEndpointLen) {
    trimmed.remove(kMaxEndpointLen);
  }
  return trimmed;
}

bool AppPreferences::isValidApiEndpoint(const String& url) {
  const String trimmed = trimEndpoint(url);
  return trimmed.startsWith("http://") || trimmed.startsWith("https://");
}

String AppPreferences::loadApiEndpoint() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return "";
  }
  const String endpoint = prefs.getString(kApiEndpointKey, "");
  prefs.end();
  return endpoint;
}

bool AppPreferences::saveApiEndpoint(const String& url) {
  const String trimmed = trimEndpoint(url);
  if (!isValidApiEndpoint(trimmed)) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  const size_t written = prefs.putString(kApiEndpointKey, trimmed);
  prefs.end();
  return written > 0;
}

bool AppPreferences::clearApiEndpoint() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  prefs.remove(kApiEndpointKey);
  prefs.end();
  return true;
}

String AppPreferences::defaultApPin() { return "12340000"; }

bool AppPreferences::isValidApPin(const String& pin) {
  const size_t len = pin.length();
  return len >= kMinApPinLen && len <= kMaxApPinLen;
}

String AppPreferences::loadApPin() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return defaultApPin();
  }
  const String pin = prefs.getString(kApPinKey, defaultApPin().c_str());
  prefs.end();
  return isValidApPin(pin) ? pin : defaultApPin();
}

bool AppPreferences::saveApPin(const String& pin) {
  if (!isValidApPin(pin)) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  const size_t written = prefs.putString(kApPinKey, pin);
  prefs.end();
  return written > 0;
}

bool AppPreferences::isValidPingIntervalSec(uint16_t seconds) {
  return seconds == 0 ||
         (seconds >= kMinPingIntervalSec && seconds <= kMaxPingIntervalSec);
}

uint16_t AppPreferences::loadPingIntervalSec() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return kDefaultPingIntervalSec;
  }
  const uint16_t value = prefs.getUShort(kPingIntervalKey, kDefaultPingIntervalSec);
  prefs.end();
  return isValidPingIntervalSec(value) ? value : kDefaultPingIntervalSec;
}

bool AppPreferences::savePingIntervalSec(uint16_t seconds) {
  if (!isValidPingIntervalSec(seconds)) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  const size_t written = prefs.putUShort(kPingIntervalKey, seconds);
  prefs.end();
  return written > 0;
}

#ifndef APP_PREFERENCES_H
#define APP_PREFERENCES_H

#include <Arduino.h>
#include <Preferences.h>

struct WifiCredentials {
  String ssid;
  String password;
  bool valid;
};

struct WelcomeMessage {
  String line1;
  String line2;
  String line3;
};

class AppPreferences {
 public:
  AppPreferences();

  static const size_t kMaxWelcomeLineLen = 24;
  static const size_t kMaxEndpointLen = 256;
  static const size_t kMinApPinLen = 8;
  static const size_t kMaxApPinLen = 63;
  static const uint16_t kDefaultPingIntervalSec = 30;
  static const uint16_t kMinPingIntervalSec = 5;
  static const uint16_t kMaxPingIntervalSec = 3600;

  bool begin();
  WifiCredentials loadWifiCredentials() const;
  bool saveWifiCredentials(const String& ssid, const String& password);
  bool clearWifiCredentials();
  WelcomeMessage loadWelcomeMessage() const;
  bool saveWelcomeMessage(const WelcomeMessage& message);
  static WelcomeMessage defaultWelcomeMessage();

  String loadApiEndpoint() const;
  bool saveApiEndpoint(const String& url);
  bool clearApiEndpoint();
  static bool isValidApiEndpoint(const String& url);

  String loadApPin() const;
  bool saveApPin(const String& pin);
  static String defaultApPin();
  static bool isValidApPin(const String& pin);

  uint16_t loadPingIntervalSec() const;
  bool savePingIntervalSec(uint16_t seconds);
  static bool isValidPingIntervalSec(uint16_t seconds);

  static const size_t kMaxWsTokenLen = 64;
  String loadWsToken() const;
  bool saveWsToken(const String& token);

 private:
  static const char* kNamespace;
  static const char* kSsidKey;
  static const char* kPassKey;
  static const char* kWelcomeLine1Key;
  static const char* kWelcomeLine2Key;
  static const char* kWelcomeLine3Key;
  static const char* kApiEndpointKey;
  static const char* kApPinKey;
  static const char* kPingIntervalKey;
  static const char* kWsTokenKey;

  static String trimWelcomeLine(const String& value);
  static String trimEndpoint(const String& value);
};

#endif

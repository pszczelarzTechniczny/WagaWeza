#ifndef MEASUREMENT_SENDER_H
#define MEASUREMENT_SENDER_H

#include <Arduino.h>

#include "../rtc/RtcClock.h"
#include "../storage/AppPreferences.h"

class WifiConnectionManager;

struct MeasurementPostResult {
  bool success;
  int httpStatus;
  String error;
};

class MeasurementSender {
 public:
  using DisplayCallback = void (*)(const char* line1, const char* line2, const char* line3,
                                   int progressPercent);
  using BuzzerCallback = void (*)(bool success);

  MeasurementSender();

  void begin(AppPreferences* appPrefs, RtcClock* rtc, DisplayCallback displayCb,
             BuzzerCallback buzzerCb);
  void setWifiManager(WifiConnectionManager* wifiManager);

  void startSend(uint8_t buttonIndex, int weightGrams);
  void tick();
  bool isActive() const;

  static String buildJsonPayload(const RtcDateTime& dt, int weightGrams, uint8_t buttonIndex);
  static MeasurementPostResult postMeasurement(const String& endpoint, const String& jsonBody,
                                               uint32_t httpTimeoutMs = 10000);

 private:
  enum class State : uint8_t {
    IDLE,
    VALIDATE,
    WIFI_ENSURE,
    HTTP_POST,
    SHOW_RESULT,
    CLEANUP,
  };

  void setState(State state);
  void show(const char* line1, const char* line2, const char* line3 = nullptr, int progress = -1);
  void finishWithError(const char* line1, const char* line2, const char* line3 = nullptr);

  AppPreferences* appPrefs_;
  RtcClock* rtc_;
  DisplayCallback displayCb_;
  BuzzerCallback buzzerCb_;

  State state_;
  bool active_;
  unsigned long stateEnteredMs_;
  unsigned long wifiEnsureStartedMs_;

  uint8_t buttonIndex_;
  int weightGrams_;
  String endpoint_;
  String jsonBody_;
  MeasurementPostResult postResult_;

  static const uint32_t kWifiEnsureTimeoutMs = 12000;
  static const uint32_t kHttpTimeoutMs = 10000;
  static const unsigned long kResultDisplayMs = 2000;
};

#endif

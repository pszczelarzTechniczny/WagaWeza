#ifndef PING_SENDER_H
#define PING_SENDER_H

#include <Arduino.h>

#include "../storage/AppPreferences.h"

class PosLink;

// Okresowa telemetria (fw, rssi, uptime, heap) po WebSocket.
class PingSender {
 public:
  PingSender();

  void begin(AppPreferences* appPrefs, const char* fwVersion);
  void tick(PosLink& link, bool measurementActive);

 private:
  AppPreferences* appPrefs_;
  String fwVersion_;
  unsigned long lastPingMs_;
  uint16_t intervalSec_;
};

#endif

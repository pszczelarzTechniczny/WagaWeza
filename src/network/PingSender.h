#ifndef PING_SENDER_H
#define PING_SENDER_H

#include <Arduino.h>

#include "../storage/AppPreferences.h"

class PingSender {
 public:
  PingSender();

  void begin(AppPreferences* appPrefs);
  void tick(bool wifiConnected, bool measurementActive);

 private:
  bool sendPing(const String& endpoint);

  AppPreferences* appPrefs_;
  unsigned long lastPingMs_;
  uint16_t intervalSec_;

  static const uint32_t kHttpTimeoutMs = 10000;
};

#endif

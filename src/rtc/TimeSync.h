#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>

#include "RtcClock.h"
#include "../storage/AppPreferences.h"

class TimeSync {
 public:
  static void configurePolandTimeZone();

  static bool connectWifi(const WifiCredentials& creds, unsigned long timeoutMs);
  static bool fetchLocalTime(RtcDateTime& out, unsigned long timeoutMs = 15000);
  static bool syncRtcFromNtp(RtcClock& rtc, const WifiCredentials& creds,
                             unsigned long wifiTimeoutMs = 12000,
                             unsigned long ntpTimeoutMs = 15000);
};

#endif

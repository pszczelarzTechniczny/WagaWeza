#ifndef CLOCK_SERVICE_ACTIONS_H
#define CLOCK_SERVICE_ACTIONS_H

#include <Arduino.h>

#include "../rtc/RtcClock.h"
#include "../storage/AppPreferences.h"

class ClockServiceActions {
 public:
  ClockServiceActions(RtcClock& rtc, AppPreferences& appPrefs);

  String currentTimeText() const;
  bool currentTimeLines(String& dateLine, String& timeLine) const;
  String syncFromInternet(unsigned long wifiTimeoutMs = 12000);
  String setManual(int year, int month, int day, int hour, int minute, int second = 0);
  bool readRtc(RtcDateTime& out) const;
  bool writeRtc(const RtcDateTime& value);

 private:
  RtcClock& rtc_;
  AppPreferences& appPrefs_;
};

#endif

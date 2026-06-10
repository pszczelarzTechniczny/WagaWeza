#ifndef POLAND_TIME_H
#define POLAND_TIME_H

#include <Arduino.h>
#include <time.h>

#include "RtcClock.h"

class PolandTime {
 public:
  // Czas letni (CEST): ostatnia niedziela marca, 01:00 UTC — ostatnia niedziela pazdziernika, 01:00 UTC
  static bool isDaylightSavingUtc(time_t utc);
  static bool utcToRtcDateTime(time_t utc, RtcDateTime& out);

  static int lastSundayOfMonth(int year, int month);
};

#endif

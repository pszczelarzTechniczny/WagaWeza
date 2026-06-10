#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <Arduino.h>

struct RtcDateTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

class RtcClock {
 public:
  bool begin();
  bool isPresent() const;

  bool read(RtcDateTime& out) const;
  bool write(const RtcDateTime& value);
  bool lostPower() const;

  static String format(const RtcDateTime& dt);
  static String formatDate(const RtcDateTime& dt);
  static String formatTime(const RtcDateTime& dt);
  static String formatIso8601(const RtcDateTime& dt);
  static bool isValid(const RtcDateTime& dt);

 private:
  static const uint8_t kAddress = 0x68;

  bool present_;
};

#endif

#include "PolandTime.h"

namespace {

int dayOfWeek(int year, int month, int day) {
  if (month < 3) {
    month += 12;
    year -= 1;
  }
  const int k = year % 100;
  const int j = year / 100;
  return (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
}

int daysInMonth(int year, int month) {
  static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  return lengths[month - 1];
}

void breakUtc(time_t utc, int& year, int& month, int& day, int& hour, int& minute, int& second) {
  struct tm utcTm {};
  gmtime_r(&utc, &utcTm);
  year = utcTm.tm_year + 1900;
  month = utcTm.tm_mon + 1;
  day = utcTm.tm_mday;
  hour = utcTm.tm_hour;
  minute = utcTm.tm_min;
  second = utcTm.tm_sec;
}

}  // namespace

int PolandTime::lastSundayOfMonth(int year, int month) {
  int day = daysInMonth(year, month);
  while (dayOfWeek(year, month, day) != 0) {
    --day;
  }
  return day;
}

bool PolandTime::isDaylightSavingUtc(time_t utc) {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  breakUtc(utc, year, month, day, hour, minute, second);
  (void)minute;
  (void)second;

  const int dstStartDay = lastSundayOfMonth(year, 3);
  const int dstEndDay = lastSundayOfMonth(year, 10);

  if (month < 3 || month > 10) {
    return false;
  }
  if (month > 3 && month < 10) {
    return true;
  }
  if (month == 3) {
    if (day > dstStartDay) {
      return true;
    }
    if (day < dstStartDay) {
      return false;
    }
    return hour >= 1;
  }
  // Pazdziernik
  if (day < dstEndDay) {
    return true;
  }
  if (day > dstEndDay) {
    return false;
  }
  return hour < 1;
}

bool PolandTime::utcToRtcDateTime(time_t utc, RtcDateTime& out) {
  const int offsetSec = isDaylightSavingUtc(utc) ? 2 * 3600 : 3600;
  const time_t local = utc + offsetSec;

  struct tm localTm {};
  gmtime_r(&local, &localTm);

  out.year = static_cast<uint16_t>(localTm.tm_year + 1900);
  out.month = static_cast<uint8_t>(localTm.tm_mon + 1);
  out.day = static_cast<uint8_t>(localTm.tm_mday);
  out.hour = static_cast<uint8_t>(localTm.tm_hour);
  out.minute = static_cast<uint8_t>(localTm.tm_min);
  out.second = static_cast<uint8_t>(localTm.tm_sec);
  return RtcClock::isValid(out);
}

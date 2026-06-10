#include "RtcClock.h"

#include <Wire.h>

namespace {

uint8_t bcdToDec(uint8_t value) { return ((value >> 4) * 10) + (value & 0x0F); }

uint8_t decToBcd(uint8_t value) { return ((value / 10) << 4) | (value % 10); }

}  // namespace

bool RtcClock::begin() {
  present_ = false;

  Wire.beginTransmission(kAddress);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  present_ = true;
  return true;
}

bool RtcClock::isPresent() const { return present_; }

bool RtcClock::read(RtcDateTime& out) const {
  if (!present_) {
    return false;
  }

  Wire.beginTransmission(kAddress);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(static_cast<int>(kAddress), 7) != 7) {
    return false;
  }

  out.second = bcdToDec(Wire.read() & 0x7F);
  out.minute = bcdToDec(Wire.read());
  out.hour = bcdToDec(Wire.read() & 0x3F);
  Wire.read();  // day of week
  out.day = bcdToDec(Wire.read());
  out.month = bcdToDec(Wire.read() & 0x1F);
  const uint8_t yearBcd = Wire.read();
  out.year = 2000 + bcdToDec(yearBcd);

  return isValid(out);
}

bool RtcClock::write(const RtcDateTime& value) {
  if (!present_ || !isValid(value)) {
    return false;
  }

  const uint8_t yearOffset = (value.year >= 2000) ? static_cast<uint8_t>(value.year - 2000) : 0;

  Wire.beginTransmission(kAddress);
  Wire.write(0x00);
  Wire.write(decToBcd(value.second));
  Wire.write(decToBcd(value.minute));
  Wire.write(decToBcd(value.hour));
  Wire.write(decToBcd(1));  // Monday — nieużywane przy wyświetlaniu
  Wire.write(decToBcd(value.day));
  Wire.write(decToBcd(value.month));
  Wire.write(decToBcd(yearOffset));
  if (Wire.endTransmission() != 0) {
    return false;
  }

  Wire.beginTransmission(kAddress);
  Wire.write(0x0F);
  Wire.write(0x00);
  return Wire.endTransmission() == 0;
}

bool RtcClock::lostPower() const {
  if (!present_) {
    return true;
  }

  Wire.beginTransmission(kAddress);
  Wire.write(0x0F);
  if (Wire.endTransmission(false) != 0) {
    return true;
  }

  if (Wire.requestFrom(static_cast<int>(kAddress), 1) != 1) {
    return true;
  }

  const uint8_t status = Wire.read();
  return (status & 0x80) != 0;
}

String RtcClock::format(const RtcDateTime& dt) {
  return formatDate(dt) + " " + formatTime(dt);
}

String RtcClock::formatDate(const RtcDateTime& dt) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%02u.%02u.%04u", dt.day, dt.month, dt.year);
  return String(buf);
}

String RtcClock::formatTime(const RtcDateTime& dt) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%02u:%02u", dt.hour, dt.minute);
  return String(buf);
}

String RtcClock::formatIso8601(const RtcDateTime& dt) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02uT%02u:%02u:%02u", dt.year, dt.month, dt.day, dt.hour,
           dt.minute, dt.second);
  return String(buf);
}

bool RtcClock::isValid(const RtcDateTime& dt) {
  if (dt.year < 2020 || dt.year > 2099) {
    return false;
  }
  if (dt.month < 1 || dt.month > 12) {
    return false;
  }
  if (dt.day < 1 || dt.day > 31) {
    return false;
  }
  if (dt.hour > 23 || dt.minute > 59 || dt.second > 59) {
    return false;
  }
  return true;
}

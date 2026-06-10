#include "ClockServiceActions.h"

#include "../rtc/TimeSync.h"

ClockServiceActions::ClockServiceActions(RtcClock& rtc, AppPreferences& appPrefs)
    : rtc_(rtc), appPrefs_(appPrefs) {}

String ClockServiceActions::currentTimeText() const {
  RtcDateTime dt;
  if (!rtc_.read(dt)) {
    return "Brak RTC";
  }
  return RtcClock::format(dt);
}

bool ClockServiceActions::currentTimeLines(String& dateLine, String& timeLine) const {
  RtcDateTime dt;
  if (!rtc_.read(dt)) {
    dateLine = "Brak RTC";
    timeLine = "";
    return false;
  }
  dateLine = RtcClock::formatDate(dt);
  timeLine = RtcClock::formatTime(dt);
  return true;
}

String ClockServiceActions::syncFromInternet(unsigned long wifiTimeoutMs) {
  if (!rtc_.isPresent()) {
    return "Brak modulu DS3231";
  }

  const WifiCredentials wifi = appPrefs_.loadWifiCredentials();
  if (!wifi.valid) {
    return "Brak zapisanej sieci WiFi";
  }

  if (!TimeSync::connectWifi(wifi, wifiTimeoutMs)) {
    return "Nie polaczono z WiFi";
  }

  RtcDateTime dt;
  if (!TimeSync::fetchLocalTime(dt)) {
    return "Blad NTP (Polska)";
  }

  if (!rtc_.write(dt)) {
    return "Blad zapisu DS3231";
  }

  return String("Zapisano: ") + RtcClock::format(dt);
}

String ClockServiceActions::setManual(int year, int month, int day, int hour, int minute,
                                      int second) {
  if (!rtc_.isPresent()) {
    return "Brak modulu DS3231";
  }

  RtcDateTime dt;
  dt.year = static_cast<uint16_t>(year);
  dt.month = static_cast<uint8_t>(month);
  dt.day = static_cast<uint8_t>(day);
  dt.hour = static_cast<uint8_t>(hour);
  dt.minute = static_cast<uint8_t>(minute);
  dt.second = static_cast<uint8_t>(second);

  if (!RtcClock::isValid(dt)) {
    return "Nieprawidlowa data lub godzina";
  }

  if (!rtc_.write(dt)) {
    return "Blad zapisu DS3231";
  }

  return String("Zapisano: ") + RtcClock::format(dt);
}

bool ClockServiceActions::readRtc(RtcDateTime& out) const { return rtc_.read(out); }

bool ClockServiceActions::writeRtc(const RtcDateTime& value) { return rtc_.write(value); }

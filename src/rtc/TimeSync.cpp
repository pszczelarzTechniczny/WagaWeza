#include "TimeSync.h"

#include <WiFi.h>
#include <time.h>

#include "PolandTime.h"

void TimeSync::configurePolandTimeZone() {
  setenv("TZ", "CET-1CEST-2,M3.5.0,M10.5.0/3", 1);
  tzset();
}

bool TimeSync::connectWifi(const WifiCredentials& creds, unsigned long timeoutMs) {
  if (!creds.valid || creds.ssid.length() == 0) {
    return false;
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == creds.ssid) {
    return true;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(creds.ssid.c_str(), creds.password.c_str());

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      return false;
    }
    delay(100);
  }
  return true;
}

bool TimeSync::fetchLocalTime(RtcDateTime& out, unsigned long timeoutMs) {
  configurePolandTimeZone();
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  const unsigned long start = millis();
  time_t utc = 0;
  while (utc < 1609459200) {
    time(&utc);
    if (utc >= 1609459200) {
      break;
    }
    if (millis() - start > timeoutMs) {
      return false;
    }
    delay(250);
  }

  return PolandTime::utcToRtcDateTime(utc, out);
}

bool TimeSync::syncRtcFromNtp(RtcClock& rtc, const WifiCredentials& creds, unsigned long wifiTimeoutMs,
                              unsigned long ntpTimeoutMs) {
  if (!rtc.isPresent()) {
    return false;
  }
  if (!connectWifi(creds, wifiTimeoutMs)) {
    return false;
  }

  RtcDateTime local;
  if (!fetchLocalTime(local, ntpTimeoutMs)) {
    return false;
  }

  return rtc.write(local);
}

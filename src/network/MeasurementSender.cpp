#include "MeasurementSender.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "WifiConnectionManager.h"

namespace {

WifiConnectionManager* gWifiForPost = nullptr;

void logOutgoingJson(const String& endpoint, const String& jsonBody) {
  Serial.println(F("--- Wysylka JSON ---"));
  Serial.print(F("POST "));
  Serial.println(endpoint);
  Serial.println(jsonBody);
  Serial.println(F("--------------------"));
}

MeasurementPostResult postJson(const String& endpoint, const String& jsonBody, uint32_t httpTimeoutMs) {
  MeasurementPostResult result;
  result.success = false;
  result.httpStatus = -1;
  result.error = "";

  if (WiFi.status() != WL_CONNECTED) {
    result.error = "Brak WiFi";
    return result;
  }

  const bool useTls = endpoint.startsWith("https://");
  HTTPClient http;
  http.setTimeout(httpTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  bool began = false;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;

  if (useTls) {
    secureClient.setInsecure();
    began = http.begin(secureClient, endpoint);
  } else {
    began = http.begin(plainClient, endpoint);
  }

  if (!began) {
    result.error = "HTTP begin failed";
    return result;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "waga-wezy-esp32");
  logOutgoingJson(endpoint, jsonBody);
  result.httpStatus = http.POST(jsonBody);
  http.getString();
  http.end();

  if (result.httpStatus >= 200 && result.httpStatus < 300) {
    result.success = true;
  } else {
    result.error = "HTTP " + String(result.httpStatus);
  }
  return result;
}

}  // namespace

MeasurementSender::MeasurementSender()
    : appPrefs_(nullptr),
      rtc_(nullptr),
      displayCb_(nullptr),
      buzzerCb_(nullptr),
      state_(State::IDLE),
      active_(false),
      stateEnteredMs_(0),
      wifiEnsureStartedMs_(0),
      buttonIndex_(0),
      weightGrams_(0),
      postResult_() {}

void MeasurementSender::begin(AppPreferences* appPrefs, RtcClock* rtc, DisplayCallback displayCb,
                              BuzzerCallback buzzerCb) {
  appPrefs_ = appPrefs;
  rtc_ = rtc;
  displayCb_ = displayCb;
  buzzerCb_ = buzzerCb;
}

void MeasurementSender::setWifiManager(WifiConnectionManager* wifiManager) {
  gWifiForPost = wifiManager;
}

void MeasurementSender::startSend(uint8_t buttonIndex, int weightGrams) {
  if (active_ || buttonIndex >= 6) {
    return;
  }
  buttonIndex_ = buttonIndex;
  weightGrams_ = weightGrams;
  active_ = true;
  setState(State::VALIDATE);
}

bool MeasurementSender::isActive() const { return active_; }

void MeasurementSender::setState(State state) {
  state_ = state;
  stateEnteredMs_ = millis();
  if (state == State::WIFI_ENSURE) {
    wifiEnsureStartedMs_ = 0;
  }
}

void MeasurementSender::show(const char* line1, const char* line2, const char* line3, int progress) {
  if (displayCb_ != nullptr) {
    displayCb_(line1, line2, line3, progress);
  }
}

void MeasurementSender::finishWithError(const char* line1, const char* line2, const char* line3) {
  show(line1, line2, line3);
  if (buzzerCb_ != nullptr) {
    buzzerCb_(false);
  }
  setState(State::SHOW_RESULT);
}

String MeasurementSender::buildJsonPayload(const RtcDateTime& dt, int weightGrams,
                                           uint8_t buttonIndex) {
  const String timestamp = RtcClock::formatIso8601(dt);
  const uint8_t buttonNumber = buttonIndex + 1;

  String json;
  json.reserve(96);
  json += "{\"timestamp\":\"";
  json += timestamp;
  json += "\",\"weightGrams\":";
  json += weightGrams;
  json += ",\"buttonNumber\":";
  json += buttonNumber;
  json += "}";
  return json;
}

MeasurementPostResult MeasurementSender::postMeasurement(const String& endpoint,
                                                         const String& jsonBody,
                                                         uint32_t httpTimeoutMs) {
  if (gWifiForPost != nullptr && !gWifiForPost->isConnected()) {
    gWifiForPost->ensureConnected(kWifiEnsureTimeoutMs);
  }
  return postJson(endpoint, jsonBody, httpTimeoutMs);
}

void MeasurementSender::tick() {
  if (!active_) {
    return;
  }

  switch (state_) {
    case State::IDLE:
      active_ = false;
      return;

    case State::VALIDATE: {
      endpoint_ = appPrefs_->loadApiEndpoint();
      if (!AppPreferences::isValidApiEndpoint(endpoint_)) {
        finishWithError("Brak adresu", "Menu serwisowe");
        break;
      }

      if (rtc_ == nullptr || !rtc_->isPresent()) {
        finishWithError("Brak DS3231", "Podlacz RTC", "I2C SDA/SCL");
        break;
      }

      RtcDateTime dt;
      if (!rtc_->read(dt) || !RtcClock::isValid(dt)) {
        finishWithError("Nie ustawiono", "zegar RTC", "Menu serwisowe");
        break;
      }

      jsonBody_ = buildJsonPayload(dt, weightGrams_, buttonIndex_);
      setState(State::WIFI_ENSURE);
    } break;

    case State::WIFI_ENSURE:
      if (gWifiForPost != nullptr && gWifiForPost->isConnected()) {
        show("Wysylam", ("Przycisk " + String(buttonIndex_ + 1)).c_str());
        setState(State::HTTP_POST);
        break;
      }

      if (wifiEnsureStartedMs_ == 0) {
        show("Laczenie WiFi", "...");
        wifiEnsureStartedMs_ = millis();
      }

      if (gWifiForPost != nullptr && gWifiForPost->ensureConnected(kWifiEnsureTimeoutMs)) {
        show("Wysylam", ("Przycisk " + String(buttonIndex_ + 1)).c_str());
        setState(State::HTTP_POST);
      } else if (millis() - wifiEnsureStartedMs_ > kWifiEnsureTimeoutMs) {
        finishWithError("Blad wysylki", "WiFi timeout");
      }
      break;

    case State::HTTP_POST: {
      postResult_ = postJson(endpoint_, jsonBody_, kHttpTimeoutMs);

      if (postResult_.success) {
        show("Wyslano", String(postResult_.httpStatus).c_str());
        if (buzzerCb_ != nullptr) {
          buzzerCb_(true);
        }
      } else {
        const char* detail =
            postResult_.error.length() > 0 ? postResult_.error.c_str() : "Blad HTTP";
        finishWithError("Blad wysylki", detail);
      }
      setState(State::SHOW_RESULT);
    } break;

    case State::SHOW_RESULT:
      if (millis() - stateEnteredMs_ >= kResultDisplayMs) {
        setState(State::CLEANUP);
      }
      break;

    case State::CLEANUP:
      active_ = false;
      state_ = State::IDLE;
      break;
  }
}

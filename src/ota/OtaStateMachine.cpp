#include "OtaStateMachine.h"

#include <WiFi.h>
#include <esp_partition.h>

OtaStateMachine* OtaStateMachine::progressContext_ = nullptr;

OtaStateMachine::OtaStateMachine()
    : prefs_(nullptr),
      displayCb_(nullptr),
      startButtonCb_(nullptr),
      state_(STATE_IDLE),
      active_(false),
      autoConfirm_(false),
      stateChangedAtMs_(0),
      wifiConnectStartedAtMs_(0),
      noUpdateShownAtMs_(0),
      lastStartButtonState_(false),
      installProgress_(0) {}

void OtaStateMachine::begin(const OtaConfig& config,
                            AppPreferences* prefs,
                            DisplayCallback displayCb,
                            ButtonReadCallback startButtonCb) {
  config_ = config;
  prefs_ = prefs;
  displayCb_ = displayCb;
  startButtonCb_ = startButtonCb;
}

void OtaStateMachine::requestStart() {
  if (active_) {
    return;
  }
  active_ = true;
  autoConfirm_ = false;
  setState(STATE_ENTER);
}

void OtaStateMachine::requestStartAuto() {
  if (active_) {
    return;
  }
  active_ = true;
  autoConfirm_ = true;
  setState(STATE_ENTER);
}

void OtaStateMachine::tick() {
  if (!active_) {
    return;
  }

  switch (state_) {
    case STATE_IDLE:
      active_ = false;
      return;

    case STATE_ENTER:
      show("Aktualizacja OTA", "Przygotowanie...", nullptr);
      setState(STATE_CHECK_PARTITIONS);
      break;

    case STATE_CHECK_PARTITIONS:
      show("Aktualizacja OTA", "Sprawdzam flash...", nullptr);
      if (!hasOtaPartitions()) {
        show("Brak partycji OTA", "Ustaw partition OTA", nullptr);
        setState(STATE_EXIT);
      } else {
        setState(STATE_LOAD_WIFI);
      }
      break;

    case STATE_LOAD_WIFI:
      wifiCredentials_ = prefs_->loadWifiCredentials();
      if (!wifiCredentials_.valid) {
        setState(STATE_START_PORTAL);
      } else {
        setState(STATE_WIFI_CONNECT_START);
      }
      break;

    case STATE_START_PORTAL:
      show("Aktualizacja OTA", "Polacz z AP WiFi", nullptr);
      if (!portal_.begin(config_.apName.c_str(), prefs_->loadApPin().c_str(),
                         config_.portalTimeoutMs)) {
        show("Blad AP", "Powrot do menu", nullptr);
        setState(STATE_EXIT);
      } else {
        setState(STATE_RUN_PORTAL);
      }
      break;

    case STATE_RUN_PORTAL: {
      show("Aktualizacja OTA", "Czekam na WiFi...", nullptr);
      const WifiConfigPortal::Result portalResult = portal_.tick();
      if (portalResult == WifiConfigPortal::RESULT_SAVED) {
        const String newSsid = portal_.savedSsid();
        const String newPassword = portal_.savedPassword();
        portal_.stop();
        prefs_->saveWifiCredentials(newSsid, newPassword);
        wifiCredentials_.ssid = newSsid;
        wifiCredentials_.password = newPassword;
        wifiCredentials_.valid = true;
        setState(STATE_WIFI_CONNECT_START);
      } else if (portalResult == WifiConfigPortal::RESULT_TIMEOUT) {
        portal_.stop();
        show("Portal timeout", "Powrot do menu", nullptr);
        setState(STATE_EXIT);
      }
    } break;

    case STATE_WIFI_CONNECT_START:
      if (!wifiCredentials_.valid) {
        setState(STATE_START_PORTAL);
        break;
      }
      if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == wifiCredentials_.ssid) {
        show("Aktualizacja OTA", "WiFi polaczone", wifiCredentials_.ssid.c_str());
        setState(STATE_WIFI_CONNECTED_INFO);
        break;
      }
      show("Aktualizacja OTA", "Laczenie WiFi...", nullptr);
      WiFi.mode(WIFI_STA);
      WiFi.begin(wifiCredentials_.ssid.c_str(), wifiCredentials_.password.c_str());
      wifiConnectStartedAtMs_ = millis();
      setState(STATE_WIFI_CONNECT_WAIT);
      break;

    case STATE_WIFI_CONNECT_WAIT:
      show("Aktualizacja OTA", wifiCredentials_.ssid.c_str(), "Laczenie...");
      if (WiFi.status() == WL_CONNECTED) {
        show("Aktualizacja OTA", "WiFi polaczone", wifiCredentials_.ssid.c_str());
        setState(STATE_WIFI_CONNECTED_INFO);
      } else if (millis() - wifiConnectStartedAtMs_ > config_.wifiConnectTimeoutMs) {
        prefs_->clearWifiCredentials();
        wifiCredentials_.valid = false;
        setState(STATE_START_PORTAL);
      }
      break;

    case STATE_WIFI_CONNECTED_INFO:
      show("Aktualizacja OTA", "Szukam wersji...", nullptr);
      if (millis() - stateChangedAtMs_ > 1500) {
        setState(STATE_CHECK_UPDATES);
      }
      break;

    case STATE_CHECK_UPDATES:
      show("Aktualizacja OTA", "GitHub...", nullptr);
      if (config_.githubOwner.length() == 0 || config_.githubRepo.length() == 0 ||
          config_.githubOwner.startsWith("YOUR_") || config_.githubRepo.startsWith("YOUR_")) {
        show("Ustaw owner/repo", "w kodzie OTA", nullptr);
        setState(STATE_EXIT);
        break;
      }
      if (!githubClient_.checkForUpdate(config_.githubOwner,
                                        config_.githubRepo,
                                        config_.currentVersion,
                                        releaseInfo_)) {
        prefs_->clearWifiCredentials();
        wifiCredentials_.valid = false;
        setState(STATE_START_PORTAL);
        break;
      }

      if (!releaseInfo_.hasUpdate) {
        show("Brak aktualizacji", "Masz najnowsza", nullptr);
        noUpdateShownAtMs_ = millis();
        setState(STATE_NO_UPDATE);
      } else {
        {
          String line2 = String("Nowa: ") + releaseInfo_.version.c_str();
          String line3 = String("Obecna: ") + config_.currentVersion.c_str();
          show("Jest aktualizacja!", line2.c_str(), line3.c_str());
        }
        setState(STATE_UPDATE_AVAILABLE_INFO);
      }
      break;

    case STATE_UPDATE_AVAILABLE_INFO: {
      String line1 = String("Obecna: ") + config_.currentVersion.c_str();
      String line2 = String("Nowa: ") + releaseInfo_.version.c_str();
      show("Jest aktualizacja!", line1.c_str(), line2.c_str());
      if (millis() - stateChangedAtMs_ > 3000) {
        setState(STATE_WAIT_START_CONFIRM);
      }
    } break;

    case STATE_WAIT_START_CONFIRM: {
      String line2 = config_.currentVersion + String(" -> ") + releaseInfo_.version;
      if (autoConfirm_) {
        show("Aktualizacja z POS", line2.c_str(), "Start...");
        setState(STATE_INSTALL);
        break;
      }
      show("Gotowe do instalacji", line2.c_str(), "Wcisnij OK");
      if (consumeStartButtonPress()) {
        setState(STATE_INSTALL);
      }
    } break;

    case STATE_INSTALL: {
      installProgress_ = 0;
      installVersionLine_ = config_.currentVersion + String(" -> ") + releaseInfo_.version;
      show("Aktualizacja OTA", installVersionLine_.c_str(), "Pobieranie...", 0);
      progressContext_ = this;
      String error;
      const bool ok = githubClient_.installFromUrl(releaseInfo_.binUrl, staticInstallProgress, error);
      progressContext_ = nullptr;

      if (ok) {
        show("Aktualizacja OK", "Restart...", nullptr);
        delay(800);
        ESP.restart();
      } else {
        show("Blad OTA", error.length() > 0 ? error.c_str() : "Powrot do AP", nullptr);
        delay(1500);
        prefs_->clearWifiCredentials();
        wifiCredentials_.valid = false;
        setState(STATE_START_PORTAL);
      }
    } break;

    case STATE_NO_UPDATE:
      if (millis() - noUpdateShownAtMs_ > 3000) {
        setState(STATE_EXIT);
      }
      break;

    case STATE_EXIT:
      portal_.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      active_ = false;
      setState(STATE_IDLE);
      break;
  }
}

bool OtaStateMachine::isActive() const { return active_; }

void OtaStateMachine::setState(State newState) {
  state_ = newState;
  stateChangedAtMs_ = millis();
}

void OtaStateMachine::show(const char* line1, const char* line2, const char* line3, int progress) {
  if (displayCb_ != nullptr) {
    displayCb_(line1, line2, line3, progress);
  }
}

bool OtaStateMachine::hasOtaPartitions() const {
  const esp_partition_t* ota0 =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  const esp_partition_t* ota1 =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  return ota0 != nullptr && ota1 != nullptr;
}

bool OtaStateMachine::consumeStartButtonPress() {
  const bool current = (startButtonCb_ != nullptr) ? startButtonCb_() : false;
  bool pressedEvent = false;

  if (current && !lastStartButtonState_) {
    pressedEvent = true;
  }

  lastStartButtonState_ = current;
  return pressedEvent;
}

void OtaStateMachine::onInstallProgress(int percent) {
  installProgress_ = percent;
  char line3[24];
  snprintf(line3, sizeof(line3), "Pobieranie %d%%", installProgress_);
  show("Aktualizacja OTA", installVersionLine_.c_str(), line3, installProgress_);
}

void OtaStateMachine::staticInstallProgress(int percent) {
  if (progressContext_ != nullptr) {
    progressContext_->onInstallProgress(percent);
  }
}

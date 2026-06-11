#ifndef OTA_STATE_MACHINE_H
#define OTA_STATE_MACHINE_H

#include <Arduino.h>

#include "../storage/AppPreferences.h"
#include "../wifi/WifiConfigPortal.h"
#include "OtaGithubClient.h"

struct OtaConfig {
  String currentVersion;
  String githubOwner;
  String githubRepo;
  String apName;
  uint32_t wifiConnectTimeoutMs;
  uint32_t portalTimeoutMs;
};

class OtaStateMachine {
 public:
  typedef void (*DisplayCallback)(const char* line1, const char* line2, const char* line3,
                                  int progressPercent);
  typedef bool (*ButtonReadCallback)();

  OtaStateMachine();

  void begin(const OtaConfig& config,
             AppPreferences* prefs,
             DisplayCallback displayCb,
             ButtonReadCallback startButtonCb);

  void requestStart();
  // Start bez potwierdzania przyciskiem OK (zdalna aktualizacja z POS).
  void requestStartAuto();
  void tick();
  bool isActive() const;

 private:
  enum State {
    STATE_IDLE = 0,
    STATE_ENTER,
    STATE_CHECK_PARTITIONS,
    STATE_LOAD_WIFI,
    STATE_START_PORTAL,
    STATE_RUN_PORTAL,
    STATE_WIFI_CONNECT_START,
    STATE_WIFI_CONNECT_WAIT,
    STATE_WIFI_CONNECTED_INFO,
    STATE_CHECK_UPDATES,
    STATE_UPDATE_AVAILABLE_INFO,
    STATE_WAIT_START_CONFIRM,
    STATE_INSTALL,
    STATE_NO_UPDATE,
    STATE_EXIT
  };

  void setState(State newState);
  void show(const char* line1, const char* line2, const char* line3, int progress = -1);
  bool hasOtaPartitions() const;
  bool consumeStartButtonPress();
  void onInstallProgress(int percent);

  static OtaStateMachine* progressContext_;
  static void staticInstallProgress(int percent);

  OtaConfig config_;
  AppPreferences* prefs_;
  DisplayCallback displayCb_;
  ButtonReadCallback startButtonCb_;

  OtaGithubClient githubClient_;
  WifiConfigPortal portal_;
  WifiCredentials wifiCredentials_;

  OtaReleaseInfo releaseInfo_;

  State state_;
  bool active_;
  bool autoConfirm_;
  unsigned long stateChangedAtMs_;
  unsigned long wifiConnectStartedAtMs_;
  unsigned long noUpdateShownAtMs_;
  bool lastStartButtonState_;

  int installProgress_;
  String installVersionLine_;
};

#endif

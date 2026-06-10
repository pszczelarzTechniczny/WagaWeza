/*
 * Waga do węzy — ESP32 + HX711 + OLED I2C + 8 przycisków + WiFi OTA + DS3231.
 */

#include <Arduino.h>
#include <cstring>

#include <WiFi.h>

#include "config/Pins.h"
#include "config/Buttons.h"

#include "src/input/InputButtons.h"
#include "src/ui/Display.h"
#include "src/scale/Scale.h"
#include "src/storage/ScalePreferences.h"
#include "src/storage/AppPreferences.h"
#include "src/rtc/RtcClock.h"
#include "src/service/ScaleServiceActions.h"
#include "src/service/ClockServiceActions.h"
#include "src/service/ServicePortal.h"
#include "src/service/ServiceTimeMenu.h"
#include "src/network/WifiConnectionManager.h"
#include "src/network/MeasurementSender.h"
#include "src/network/PingSender.h"
#include "src/workflow/MeasurementWorkflow.h"
#include "src/ota/OtaStateMachine.h"
#include "src/output/Buzzer.h"

static const char* FW_VERSION = "1.0.3";
static const char* OTA_GITHUB_OWNER = "pszczelarzTechniczny";
static const char* OTA_GITHUB_REPO = "WagaWeza";
static const char* OTA_AP_NAME = "WagaWezy-Setup";

static Display gDisplay;
static InputButtons gButtons;
static Scale gScale;
static ScalePreferences gScalePrefs;
static AppPreferences gAppPrefs;
static RtcClock gRtc;
static ScaleServiceActions gServiceActions(gScale, gScalePrefs);
static ClockServiceActions gClockActions(gRtc, gAppPrefs);
static ServicePortal gServicePortal(gServiceActions, gClockActions, gAppPrefs);
static ServiceTimeMenu gTimeMenu(gClockActions, gDisplay);
static WifiConnectionManager gWifiManager;
static MeasurementSender gMeasurementSender;
static PingSender gPingSender;
static MeasurementWorkflow gMeasurementWorkflow;
static OtaStateMachine gOta;
static Buzzer gBuzzer;

static bool gServiceModeActive = false;
static bool gOtaBlockingWifi = false;
static unsigned long gTaraMsgUntilMs = 0;
static unsigned long gServiceDisplayUpdatedMs = 0;

static void appDisplayStatus(const char* line1, const char* line2, const char* line3,
                             int progressPercent) {
  gDisplay.showThreeLinesLeft(line1, line2, line3, progressPercent);
}

static void measurementBuzzerFeedback(bool success) {
  if (success) {
    gBuzzer.beep(150);
  } else {
    gBuzzer.beep(100);
    delay(80);
    gBuzzer.beep(100);
  }
}

static bool otaOkButtonPressed() {
  return gButtons.isHeld(BTN_OK);
}

static bool otaConfigComplete() {
  if (OTA_GITHUB_OWNER == nullptr || OTA_GITHUB_REPO == nullptr) {
    return false;
  }
  if (strlen(OTA_GITHUB_OWNER) == 0 || strlen(OTA_GITHUB_REPO) == 0) {
    return false;
  }
  if (strncmp(OTA_GITHUB_OWNER, "YOUR_", 5) == 0 || strncmp(OTA_GITHUB_REPO, "YOUR_", 5) == 0) {
    return false;
  }
  return true;
}

static unsigned long gMainClockUpdatedMs = 0;
static char gMainClockText[8] = "";

static void refreshMainClockText() {
  const unsigned long now = millis();
  if (now - gMainClockUpdatedMs < 1000) {
    return;
  }
  gMainClockUpdatedMs = now;

  gMainClockText[0] = '\0';
  if (!gRtc.isPresent()) {
    return;
  }

  RtcDateTime dt;
  if (gRtc.read(dt) && RtcClock::isValid(dt)) {
    snprintf(gMainClockText, sizeof(gMainClockText), "%02u:%02u", dt.hour, dt.minute);
  }
}

static void processNormal() {
  refreshMainClockText();
  const int net = gScale.readNetGrams(gScale.runtimeTara());
  gDisplay.showWeight(net, gMainClockText[0] != '\0' ? gMainClockText : nullptr);
}

static void updateServiceDisplay() {
  const unsigned long now = millis();
  if (now - gServiceDisplayUpdatedMs < 2000) {
    return;
  }
  gServiceDisplayUpdatedMs = now;

  static bool showClockLine = false;
  showClockLine = !showClockLine;

  const String ip = WiFi.softAPIP().toString();
  if (showClockLine) {
    String dateLine;
    String timeLine;
    gClockActions.currentTimeLines(dateLine, timeLine);
    gDisplay.showThreeLinesLeft("Czas DS3231", dateLine.c_str(), timeLine.c_str());
  } else {
    gDisplay.showThreeLinesLeft("Tryb serwisowy", ip.c_str(), "Przycisk 3 = czas");
  }
}

static void enterServiceMode() {
  gWifiManager.setEnabled(false);
  gWifiManager.setApModeActive(true);
  if (!gServicePortal.begin(OTA_AP_NAME)) {
    gWifiManager.setApModeActive(false);
    gWifiManager.setEnabled(true);
    gDisplay.showTwoLines("Blad AP", "Sprobuj ponownie");
    delay(1500);
    return;
  }
  gServiceModeActive = true;
  gServiceDisplayUpdatedMs = 0;
  gBuzzer.beep(300);
  updateServiceDisplay();
}

static void exitServiceMode() {
  gTimeMenu.exit();
  gServicePortal.stop();
  gServiceModeActive = false;
  gServicePortal.clearExitRequest();
  gWifiManager.setApModeActive(false);
  gWifiManager.setEnabled(true);
}

static void startOtaFromService() {
  gServicePortal.stop();
  gServiceModeActive = false;
  gServicePortal.clearOtaRequest();
  gWifiManager.setApModeActive(false);
  gOtaBlockingWifi = true;
  gWifiManager.setEnabled(false);

  if (!otaConfigComplete()) {
    gWifiManager.setEnabled(true);
    gDisplay.showStatus("OTA nieaktywne", "Ustaw owner/repo", -1);
    delay(1500);
    return;
  }
  gOta.requestStart();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("WagaWezy start");

  gButtons.begin();
  gBuzzer.begin(PIN_BUZZER, PIN_BUZZER_ACTIVE_LOW);

  if (!gDisplay.begin()) {
    Serial.println("OLED init failed");
  }

  ScaleCalibrationData cal;
  gScalePrefs.load(cal);

  gScale.begin(PIN_HX711_DT, PIN_HX711_SCK);
  gScale.applyCalibration(cal);
  gAppPrefs.begin();

  if (!gRtc.begin()) {
    Serial.println("DS3231 not found on I2C");
    gDisplay.showThreeLinesLeft("Brak DS3231", "Podlacz RTC", "I2C SDA/SCL");
    delay(1500);
  } else if (gRtc.lostPower()) {
    Serial.println("DS3231 lost power — set time in service menu");
    gDisplay.showThreeLinesLeft("Nie ustawiono", "zegar RTC", "Menu serwisowe");
    delay(1500);
  }

  const WelcomeMessage welcome = gAppPrefs.loadWelcomeMessage();
  gDisplay.showWelcome(welcome);
  delay(2000);

  if (!gScale.isPresent()) {
    gDisplay.showTwoLines("Brak wagi", "Podlacz HX711");
    delay(2000);
  } else if (cal.factor == 0.0f) {
    gDisplay.showTwoLines("Nie", "skalibrowano");
    delay(2000);
  } else {
    if (gScale.bootTareIfEmpty()) {
      cal = gScale.calibrationData();
      gScalePrefs.save(cal);
    }
  }

  OtaConfig otaConfig;
  otaConfig.currentVersion = FW_VERSION;
  otaConfig.githubOwner = OTA_GITHUB_OWNER;
  otaConfig.githubRepo = OTA_GITHUB_REPO;
  otaConfig.apName = OTA_AP_NAME;
  otaConfig.wifiConnectTimeoutMs = 12000;
  otaConfig.portalTimeoutMs = 180000;
  gOta.begin(otaConfig, &gAppPrefs, appDisplayStatus, otaOkButtonPressed);

  gWifiManager.begin(&gAppPrefs);
  gMeasurementSender.begin(&gAppPrefs, &gRtc, appDisplayStatus, measurementBuzzerFeedback);
  gMeasurementSender.setWifiManager(&gWifiManager);
  gPingSender.begin(&gAppPrefs, FW_VERSION);
  gMeasurementWorkflow.begin(&gMeasurementSender, &gDisplay);

  gDisplay.showLine("Laczenie WiFi");
  gWifiManager.connectAtBoot();

  gBuzzer.beepBlocking(400);

  gDisplay.showLine("Gotowe");
  delay(500);
}

void loop() {
  gButtons.tick();
  gBuzzer.tick();

  if (gOta.isActive()) {
    gOta.tick();
    return;
  }

  if (gOtaBlockingWifi) {
    gOtaBlockingWifi = false;
    gWifiManager.setEnabled(true);
  }

  if (gTaraMsgUntilMs > millis()) {
    gDisplay.showLine("Tara");
    return;
  }

  if (gServiceModeActive) {
    gServicePortal.tick();

    if (gServicePortal.exitRequested()) {
      exitServiceMode();
      return;
    }

    if (gServicePortal.otaRequested()) {
      startOtaFromService();
      return;
    }

    if (gTimeMenu.isActive()) {
      gTimeMenu.tick(gButtons);
      return;
    }

    if (gButtons.wasPressed(BTN_3)) {
      gTimeMenu.enter();
      return;
    }

    updateServiceDisplay();
    return;
  }

  if (gButtons.areBothHeld()) {
    const int holdPercent = gButtons.serviceComboHoldPercent();
    gDisplay.showThreeLinesWithProgress("Przytrzymaj", "menu", "serwisowe", holdPercent);

    if (gButtons.serviceComboTriggered()) {
      enterServiceMode();
    }
    return;
  }

  if (gMeasurementWorkflow.tick(gButtons, gScale)) {
    return;
  }

  if (gButtons.wasPressed(BTN_TARA)) {
    gScale.setRuntimeTara(gScale.readRawGrams());
    gTaraMsgUntilMs = millis() + 500;
    gBuzzer.beep(150);
    Serial.println("Quick tara");
    return;
  }

  gWifiManager.tick();
  gPingSender.tick(gWifiManager.isConnected(), gMeasurementSender.isActive());
  processNormal();
}

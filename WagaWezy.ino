/*
 * Waga do węzy — ESP32 + HX711 + OLED I2C + 8 przycisków + WiFi OTA + DS3231.
 */

#include <Arduino.h>
#include <cstring>

#include <WiFi.h>
#include <esp_task_wdt.h>

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
#include "src/service/ServiceCalMenu.h"
#include "src/service/ServiceMenu.h"
#include "src/network/WifiConnectionManager.h"
#include "src/network/PosLink.h"
#include "src/network/PingSender.h"
#include "src/scale/StabilityTracker.h"
#include "src/workflow/MeasurementWorkflow.h"
#include "src/ota/OtaStateMachine.h"
#include "src/output/Buzzer.h"

static const char* FW_VERSION = "1.4.1";
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
static ServiceCalMenu gCalMenu(gServiceActions, gDisplay);
static ServiceMenu gServiceMenu(gDisplay);
static WifiConnectionManager gWifiManager;
static PosLink gPosLink;
static StabilityTracker gStability;
static PingSender gPingSender;
static MeasurementWorkflow gMeasurementWorkflow;
static OtaStateMachine gOta;
static Buzzer gBuzzer;

static bool gServiceModeActive = false;
static bool gOtaBlockingWifi = false;
static unsigned long gTaraMsgUntilMs = 0;
static unsigned long gOkHoldStartMs = 0;
static bool gOkUndoFired = false;
static unsigned long gUndoMsgUntilMs = 0;
static unsigned long gServiceDisplayUpdatedMs = 0;
static bool gServiceInfoActive = false;
static uint8_t gServiceInfoPage = 0;
static unsigned long gServiceMsgUntilMs = 0;
static unsigned long gTaraPersistDueMs = 0;
static char gResetReason[16] = "?";
static uint32_t gBootCount = 0;

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
  // Pętla jest teraz szybka (nieblokujący odczyt HX711) — pełny redraw OLED
  // co obieg blokowałby ją na I2C; 10 odświeżeń/s w zupełności wystarcza.
  static unsigned long lastDrawMs = 0;
  const unsigned long now = millis();
  if (now - lastDrawMs < 100) {
    return;
  }
  lastDrawMs = now;

  refreshMainClockText();

  if (!gScale.isCalibrated()) {
    gDisplay.showThreeLinesLeft("Brak kalibracji", "Wejdz do menu", "serwisowego");
    return;
  }
  if (gScale.isOverload()) {
    gDisplay.showTwoLines("PRZECIAZENIE", "Zdejmij towar");
    return;
  }
  if (gScale.isUnderRange()) {
    gDisplay.showTwoLines("Odczyt ujemny", "Wykonaj tare");
    return;
  }

  const int net = gScale.readNetGrams(gScale.runtimeTara());
  WeightStatus status;
  status.wifi = gWifiManager.isConnected();
  status.ws = gPosLink.isConnected();
  status.pos = gPosLink.posOnline();
  status.stable = gStability.isStable(millis());
  gDisplay.showWeight(net, gMainClockText[0] != '\0' ? gMainClockText : nullptr, &status);
}

static void showServiceInfo(uint8_t page) {
  char l1[24], l2[24], l3[24], l4[24], l5[24], l6[24];

  if (page == 1) {
    snprintf(l1, sizeof(l1), "FW v%s", FW_VERSION);
    snprintf(l2, sizeof(l2), "Restart: %s", gResetReason);
    snprintf(l3, sizeof(l3), "Booty: %u", static_cast<unsigned>(gBootCount));
    snprintf(l4, sizeof(l4), "WS rozl.: %u", static_cast<unsigned>(gPosLink.wsDisconnects()));
    snprintf(l5, sizeof(l5), "Ack RTT: %u ms", static_cast<unsigned>(gPosLink.lastAckRttMs()));
    snprintf(l6, sizeof(l6), "OK=wroc Tara=wyjdz");
    const char* lines[6] = {l1, l2, l3, l4, l5, l6};
    gDisplay.showInfoScreen(lines, 6);
    return;
  }

  snprintf(l1, sizeof(l1), "FW v%s", FW_VERSION);

  if (WiFi.status() == WL_CONNECTED) {
    snprintf(l2, sizeof(l2), "WiFi: %.15s", WiFi.SSID().c_str());
    snprintf(l3, sizeof(l3), "RSSI %d dBm", static_cast<int>(WiFi.RSSI()));
    snprintf(l4, sizeof(l4), "IP %s", WiFi.localIP().toString().c_str());
  } else {
    const WifiCredentials creds = gAppPrefs.loadWifiCredentials();
    if (creds.valid) {
      snprintf(l2, sizeof(l2), "WiFi: %.15s", creds.ssid.c_str());
    } else {
      snprintf(l2, sizeof(l2), "WiFi: brak konfig.");
    }
    snprintf(l3, sizeof(l3), "nie polaczono");
    snprintf(l4, sizeof(l4), "AP %s", WiFi.softAPIP().toString().c_str());
  }

  String endpoint = gAppPrefs.loadApiEndpoint();
  if (endpoint.length() > 0) {
    endpoint.replace("https://", "");
    endpoint.replace("http://", "");
    snprintf(l5, sizeof(l5), "-> %.18s", endpoint.c_str());
  } else {
    snprintf(l5, sizeof(l5), "-> brak endpointu");
  }
  snprintf(l6, sizeof(l6), "OK=wiecej Tara=wroc");

  const char* lines[6] = {l1, l2, l3, l4, l5, l6};
  gDisplay.showInfoScreen(lines, 6);
}

// Test polaczenia WS (portal + przycisk 1 w serwisie). Blokujaco: w razie
// potrzeby laczy STA, wznawia PosLink na probe i z powrotem go wstrzymuje.
static String wsTestReport() {
  if (!gPosLink.configValid()) {
    return "Brak poprawnego endpointu";
  }

  if (WiFi.status() != WL_CONNECTED) {
    const WifiCredentials wifi = gAppPrefs.loadWifiCredentials();
    if (!wifi.valid) {
      return "Brak zapisanej sieci WiFi";
    }
    gDisplay.showTwoLines("Test WS", "Laczenie WiFi...");
    WiFi.begin(wifi.ssid.c_str(), wifi.password.c_str());
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
      esp_task_wdt_reset();
      delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
      return "WiFi timeout";
    }
  }

  gDisplay.showTwoLines("Test WS", "Laczenie...");
  gPosLink.resume();
  const unsigned long start = millis();
  while (millis() - start < 6000 && !gPosLink.isConnected()) {
    esp_task_wdt_reset();
    gPosLink.tick();
    delay(20);
  }

  String result;
  if (gPosLink.isConnected()) {
    const unsigned long settle = millis();
    while (millis() - settle < 500) {
      esp_task_wdt_reset();
      gPosLink.tick();
      delay(10);
    }
    result = "Polaczono: " + gPosLink.wsUrlText();
    result += gPosLink.posOnline() ? " | POS online" : " | POS offline";
  } else {
    result = "Brak polaczenia: " + gPosLink.wsUrlText();
  }

  if (gServiceModeActive) {
    gPosLink.suspend();
  }
  return result;
}

// Streaming masy na zywo: przy zmianie odczytu lub flagi stabilnosci,
// nie czesciej niz co 100 ms (maks. ~10/s wg spec).
static void tickLiveWeight(int netGrams) {
  static int lastSentGrams = -1000000;
  static bool lastStable = false;
  static unsigned long lastSentMs = 0;

  if (!gPosLink.isConnected()) {
    return;
  }
  const unsigned long now = millis();
  const bool stable = gStability.isStable(now);
  if (now - lastSentMs < 100) {
    return;
  }
  if (netGrams == lastSentGrams && stable == lastStable) {
    return;
  }
  lastSentMs = now;
  lastSentGrams = netGrams;
  lastStable = stable;
  gPosLink.sendWeight(netGrams / 1000.0f, stable);
}

static void enterServiceMode() {
  gWifiManager.setEnabled(false);
  gWifiManager.setApModeActive(true);
  gPosLink.suspend();
  if (!gServicePortal.begin(OTA_AP_NAME)) {
    gWifiManager.setApModeActive(false);
    gWifiManager.setEnabled(true);
    gDisplay.showTwoLines("Blad AP", "Sprobuj ponownie");
    delay(1500);
    return;
  }
  gServiceModeActive = true;
  gServiceDisplayUpdatedMs = 0;
  gServiceInfoActive = false;
  gServiceMsgUntilMs = 0;
  gBuzzer.beep(300);
  gServiceMenu.enter(WiFi.softAPIP().toString().c_str());
}

static void exitServiceMode() {
  gTimeMenu.exit();
  gCalMenu.exit();
  gServiceInfoActive = false;
  gServiceMsgUntilMs = 0;
  gServicePortal.stop();
  gServiceModeActive = false;
  gServicePortal.clearExitRequest();
  gWifiManager.setApModeActive(false);
  gWifiManager.setEnabled(true);
  gPosLink.resume();
}

static void startOtaFromService() {
  gServicePortal.stop();
  gServiceModeActive = false;
  gServicePortal.clearOtaRequest();
  gPosLink.suspend();
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

// Zdalna aktualizacja wyzwolona z POS ({type:"update"} po WS): jak
// startOtaFromService, ale bez portalu i bez potwierdzania przyciskiem OK.
static void startOtaFromRemote() {
  gPosLink.suspend();
  gWifiManager.setApModeActive(false);
  gOtaBlockingWifi = true;
  gWifiManager.setEnabled(false);

  if (!otaConfigComplete()) {
    gWifiManager.setEnabled(true);
    gOtaBlockingWifi = false;
    gPosLink.resume();
    gDisplay.showStatus("OTA nieaktywne", "Ustaw owner/repo", -1);
    delay(1500);
    return;
  }
  gDisplay.showTwoLines("Aktualizacja", "z POS...");
  gOta.requestStartAuto();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("WagaWezy start");

  // Watchdog 30 s — restart przy zwisie loop()/OTA. Core esp32 3.x inicjuje
  // task WDT przy starcie, wiec init zwroci INVALID_STATE -> rekonfiguracja.
  esp_task_wdt_config_t wdtCfg = {};
  wdtCfg.timeout_ms = 30000;
  wdtCfg.idle_core_mask = 0;
  wdtCfg.trigger_panic = true;
  if (esp_task_wdt_init(&wdtCfg) == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&wdtCfg);
  }
  esp_task_wdt_add(NULL);

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
  const uint32_t bootCount = gAppPrefs.incrementBootCount();
  gBootCount = bootCount;

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
  gPosLink.begin(&gAppPrefs, FW_VERSION, &gRtc);
  gPingSender.begin(&gAppPrefs, FW_VERSION);

  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: strncpy(gResetReason, "POWERON", sizeof(gResetReason)); break;
    case ESP_RST_SW: strncpy(gResetReason, "SW", sizeof(gResetReason)); break;
    case ESP_RST_PANIC: strncpy(gResetReason, "PANIC", sizeof(gResetReason)); break;
    case ESP_RST_INT_WDT: strncpy(gResetReason, "INT_WDT", sizeof(gResetReason)); break;
    case ESP_RST_TASK_WDT: strncpy(gResetReason, "TASK_WDT", sizeof(gResetReason)); break;
    case ESP_RST_WDT: strncpy(gResetReason, "WDT", sizeof(gResetReason)); break;
    case ESP_RST_BROWNOUT: strncpy(gResetReason, "BROWNOUT", sizeof(gResetReason)); break;
    case ESP_RST_DEEPSLEEP: strncpy(gResetReason, "DEEPSLEEP", sizeof(gResetReason)); break;
    default: snprintf(gResetReason, sizeof(gResetReason), "%d", static_cast<int>(esp_reset_reason())); break;
  }
  gResetReason[sizeof(gResetReason) - 1] = '\0';
  gPosLink.setDiagnostics(gResetReason, bootCount);
  Serial.printf("[diag] reset=%s boot=%u\n", gResetReason, static_cast<unsigned>(bootCount));
  gMeasurementWorkflow.begin(&gPosLink, &gDisplay, measurementBuzzerFeedback);
  gServicePortal.setWsTestFn(wsTestReport);

  gDisplay.showLine("Laczenie WiFi");
  gWifiManager.connectAtBoot();

  gBuzzer.beepBlocking(400);

  gDisplay.showLine("Gotowe");
  delay(500);
}

void loop() {
  esp_task_wdt_reset();

  gButtons.tick();
  gBuzzer.tick();
  gScale.tick();

  if (gOta.isActive()) {
    gOta.tick();
    return;
  }

  if (gOtaBlockingWifi) {
    gOtaBlockingWifi = false;
    gWifiManager.setEnabled(true);
    gPosLink.resume();
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
      if (!gTimeMenu.tick(gButtons)) {
        gServiceMenu.requestRedraw();
      }
      return;
    }

    if (gCalMenu.isActive()) {
      if (!gCalMenu.tick(gButtons)) {
        gServiceMenu.requestRedraw();
      }
      return;
    }

    if (gServiceInfoActive) {
      if (gButtons.wasPressed(BTN_TARA)) {
        gServiceInfoActive = false;
        gServiceMenu.requestRedraw();
        return;
      }
      if (gButtons.wasPressed(BTN_OK)) {
        gServiceInfoPage ^= 1;
        gServiceDisplayUpdatedMs = millis();
        showServiceInfo(gServiceInfoPage);
        return;
      }
      const unsigned long now = millis();
      if (now - gServiceDisplayUpdatedMs >= 2000) {
        gServiceDisplayUpdatedMs = now;
        showServiceInfo(gServiceInfoPage);
      }
      return;
    }

    // Komunikat wyniku (test/tara) trzymany na ekranie do upływu czasu.
    if (gServiceMsgUntilMs != 0) {
      if (millis() < gServiceMsgUntilMs) {
        return;
      }
      gServiceMsgUntilMs = 0;
      gServiceMenu.requestRedraw();
    }

    switch (gServiceMenu.tick(gButtons)) {
      case ServiceMenuAction::TEST_WS: {
        const String msg = wsTestReport();
        gDisplay.showTwoLines("Test WS", msg.c_str());
        gServiceMsgUntilMs = millis() + 2500;
        return;
      }
      case ServiceMenuAction::INFO:
        gServiceInfoActive = true;
        gServiceInfoPage = 0;
        gServiceDisplayUpdatedMs = millis();
        showServiceInfo(gServiceInfoPage);
        return;
      case ServiceMenuAction::TIME:
        gTimeMenu.enter();
        return;
      case ServiceMenuAction::UPDATE:
        startOtaFromService();
        return;
      case ServiceMenuAction::CALIBRATION:
        gCalMenu.enter();
        return;
      case ServiceMenuAction::TARA_SAVE: {
        bool ok = false;
        const String msg = gServiceActions.saveTara(&ok);
        gDisplay.showTwoLines(ok ? "Tara" : "Blad", msg.c_str());
        gBuzzer.beep(ok ? 150 : 80);
        gServiceMsgUntilMs = millis() + 2000;
        return;
      }
      case ServiceMenuAction::EXIT:
        gBuzzer.beep(200);
        exitServiceMode();
        return;
      case ServiceMenuAction::NONE:
        break;
    }
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

  gWifiManager.tick();
  gPosLink.tick();

  const int net = gScale.readNetGrams(gScale.runtimeTara());
  gStability.feed(net, millis());
  tickLiveWeight(net);
  gPingSender.tick(gPosLink, gMeasurementWorkflow.isActive());

  // Auto-zapis szybkiej tary do NVS (debounce 5 s chroni flash). Po restarcie
  // waga wstaje z aktywna tara — applyCalibration przywraca savedTara.
  if (gTaraPersistDueMs != 0 && millis() >= gTaraPersistDueMs) {
    gTaraPersistDueMs = 0;
    if (gScale.runtimeTara() != gScale.calibrationData().savedTara) {
      ScaleCalibrationData d = gScale.calibrationData();
      d.savedTara = gScale.runtimeTara();
      gScalePrefs.save(d);
      gScale.applyCalibration(d);
      Serial.println("[tara] zapisano runtime tare do NVS");
    }
  }

  if (gMeasurementWorkflow.tick(gButtons, gScale, gStability)) {
    return;
  }

  // Zdalna aktualizacja z POS — tylko gdy nie trwa pomiar.
  if (gPosLink.takeUpdateRequest()) {
    Serial.println("[ota] zdalna aktualizacja z POS");
    startOtaFromRemote();
    return;
  }

  if (gUndoMsgUntilMs > millis()) {
    gDisplay.showTwoLines("Cofniecie", "wyslane do POS");
    return;
  }

  // OK przytrzymane ~1,5 s = undo (gdy TARA tez wcisnieta, to kombinacja
  // wejscia w serwis — nie liczymy). Jedno undo na przytrzymanie — kolejne
  // dopiero po puszczeniu przycisku.
  if (gButtons.isHeld(BTN_OK) && !gButtons.isHeld(BTN_TARA)) {
    const unsigned long now = millis();
    if (gOkHoldStartMs == 0) {
      gOkHoldStartMs = now;
    }
    if (!gOkUndoFired && now - gOkHoldStartMs >= 1500) {
      gOkUndoFired = true;
      gPosLink.sendUndo();
      gBuzzer.beep(200);
      gUndoMsgUntilMs = millis() + 1000;
      Serial.println("[pomiar] undo");
      return;
    }
    if (!gOkUndoFired) {
      const int percent = static_cast<int>((now - gOkHoldStartMs) * 100 / 1500);
      gDisplay.showThreeLinesWithProgress("Cofniecie", "pozycji z wagi", "trzymaj OK", percent);
      return;
    }
  } else {
    gOkHoldStartMs = 0;
    gOkUndoFired = false;
  }

  if (gButtons.wasPressed(BTN_TARA)) {
    gScale.setRuntimeTara(gScale.readRawGrams());
    gTaraMsgUntilMs = millis() + 500;
    gTaraPersistDueMs = millis() + 5000;
    gBuzzer.beep(150);
    Serial.println("Quick tara");
    return;
  }

  processNormal();
}

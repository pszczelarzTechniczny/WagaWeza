#include "ServicePortal.h"

#include "../network/MeasurementSender.h"

namespace {

String escapeHtmlAttr(const String& value) {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '&') {
      out += "&amp;";
    } else if (c == '"') {
      out += "&quot;";
    } else if (c == '<') {
      out += "&lt;";
    } else if (c == '>') {
      out += "&gt;";
    } else {
      out += c;
    }
  }
  return out;
}

}  // namespace

ServicePortal::ServicePortal(ScaleServiceActions& actions, ClockServiceActions& clockActions,
                             AppPreferences& appPrefs)
    : actions_(actions),
      clockActions_(clockActions),
      appPrefs_(appPrefs),
      server_(80),
      running_(false),
      exitRequested_(false),
      otaRequested_(false),
      networkCount_(0) {
  registerRoutes();
}

bool ServicePortal::begin(const char* apName) {
  stop();

  apName_ = apName;
  exitRequested_ = false;
  otaRequested_ = false;
  networkCount_ = 0;

  WiFi.mode(WIFI_AP_STA);
  const String apPin = appPrefs_.loadApPin();
  if (!WiFi.softAP(apName_.c_str(), apPin.c_str())) {
    return false;
  }

  scanNetworks();

  dnsServer_.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer_.start(53, "*", WiFi.softAPIP());

  server_.begin();
  running_ = true;
  return true;
}

void ServicePortal::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { server_.send(200, "text/html", buildPage()); });

  server_.on("/ap/save", HTTP_POST, [this]() {
    const String pin = server_.arg("ap_pin");
    if (!appPrefs_.saveApPin(pin)) {
      server_.send(400, "text/html",
                   resultPage("Blad", "PIN AP: wymagane 8-63 znaki.", true));
      return;
    }

    restartAp();
    server_.send(200, "text/html",
                 resultPage("PIN AP zapisany",
                            "Polacz ponownie z siecia AP uzywajac nowego hasla.", true));
  });

  server_.on("/wifi/save", HTTP_POST, [this]() {
    String ssid = server_.arg("ssid");
    ssid.trim();
    if (ssid.length() == 0) {
      ssid = server_.arg("manual_ssid");
      ssid.trim();
    }
    const String password = server_.arg("password");

    if (ssid.length() == 0) {
      server_.send(400, "text/plain", "SSID wymagane");
      return;
    }

    if (appPrefs_.saveWifiCredentials(ssid, password)) {
      server_.send(200, "text/html", resultPage("WiFi zapisane", "Dane sieci zapisane w pamieci.", true));
    } else {
      server_.send(500, "text/html", resultPage("Blad", "Nie udalo sie zapisac WiFi.", true));
    }
  });

  server_.on("/tara", HTTP_POST, [this]() {
    const String msg = actions_.saveTara();
    server_.send(200, "text/html", resultPage("Tara", msg.c_str(), true));
  });

  server_.on("/cal/empty", HTTP_POST, [this]() {
    const String msg = actions_.calibrateEmpty();
    server_.send(200, "text/html", resultPage("Kalibracja", msg.c_str(), true));
  });

  server_.on("/cal/weight", HTTP_POST, [this]() {
    const int grams = server_.arg("grams").toInt();
    const String msg = actions_.calibrateWithWeight(grams);
    server_.send(200, "text/html", resultPage("Kalibracja", msg.c_str(), true));
  });

  server_.on("/cal/hx711-reset", HTTP_POST, [this]() {
    const String msg = actions_.resetHx711Default();
    server_.send(200, "text/html", resultPage("HX711", msg.c_str(), true));
  });

  server_.on("/reset", HTTP_POST, [this]() {
    const String msg = actions_.resetAll();
    server_.send(200, "text/html", resultPage("Reset", msg.c_str(), true));
  });

  server_.on("/welcome/save", HTTP_POST, [this]() {
    WelcomeMessage message;
    message.line1 = server_.arg("welcome_l1");
    message.line2 = server_.arg("welcome_l2");
    message.line3 = server_.arg("welcome_l3");

    if (appPrefs_.saveWelcomeMessage(message)) {
      server_.send(200, "text/html",
                   resultPage("Ekran powitalny", "Tekst zapisany w pamieci NVS. Widoczny po restarcie.", true));
    } else {
      server_.send(500, "text/html",
                   resultPage("Blad", "Nie udalo sie zapisac ekranu powitalnego.", true));
    }
  });

  server_.on("/endpoint/save", HTTP_POST, [this]() {
    const String endpoint = server_.arg("endpoint");
    if (appPrefs_.saveApiEndpoint(endpoint)) {
      server_.send(200, "text/html",
                   resultPage("Endpoint", "Adres URL zapisany w pamieci.", true));
    } else {
      server_.send(400, "text/html",
                   resultPage("Blad", "Wymagany adres http:// lub https://", true));
    }
  });

  server_.on("/ping/save", HTTP_POST, [this]() {
    const uint16_t interval = static_cast<uint16_t>(server_.arg("ping_interval").toInt());
    if (appPrefs_.savePingIntervalSec(interval)) {
      server_.send(200, "text/html",
                   resultPage("Ping", "Interwal pinga zapisany.", true));
    } else {
      server_.send(400, "text/html",
                   resultPage("Blad", "Interwal: 0 (wyl.) lub 5-3600 sekund.", true));
    }
  });

  server_.on("/endpoint/test", HTTP_POST, [this]() {
    const String endpoint = appPrefs_.loadApiEndpoint();
    if (!AppPreferences::isValidApiEndpoint(endpoint)) {
      server_.send(400, "text/html", resultPage("Test", "Brak poprawnego endpointu.", true));
      return;
    }

    const WifiCredentials wifi = appPrefs_.loadWifiCredentials();
    if (!wifi.valid) {
      server_.send(400, "text/html", resultPage("Test", "Brak zapisanej sieci WiFi.", true));
      return;
    }

    RtcDateTime dt;
    if (!clockActions_.readRtc(dt) || !RtcClock::isValid(dt)) {
      server_.send(400, "text/html", resultPage("Test", "Brak poprawnego czasu DS3231.", true));
      return;
    }

    const int weight = actions_.currentNetWeightGrams();
    const String json = MeasurementSender::buildJsonPayload(dt, weight, 0);
    const MeasurementPostResult result = MeasurementSender::postMeasurement(endpoint, json);

    if (result.success) {
      server_.send(200, "text/html",
                   resultPage("Test OK", ("HTTP " + String(result.httpStatus)).c_str(), true));
    } else {
      const String msg = result.error.length() > 0 ? result.error : "Blad HTTP";
      server_.send(500, "text/html", resultPage("Test blad", msg.c_str(), true));
    }
  });

  server_.on("/time/ntp", HTTP_POST, [this]() {
    const String msg = clockActions_.syncFromInternet();
    server_.send(200, "text/html", resultPage("Czas z internetu", msg.c_str(), true));
  });

  server_.on("/time/set", HTTP_POST, [this]() {
    const int year = server_.arg("year").toInt();
    const int month = server_.arg("month").toInt();
    const int day = server_.arg("day").toInt();
    const int hour = server_.arg("hour").toInt();
    const int minute = server_.arg("minute").toInt();
    const String msg = clockActions_.setManual(year, month, day, hour, minute, 0);
    server_.send(200, "text/html", resultPage("Ustawienie czasu", msg.c_str(), true));
  });

  server_.on("/ota", HTTP_POST, [this]() {
    otaRequested_ = true;
    server_.send(200, "text/html",
                 resultPage("OTA", "Rozpoczynam aktualizacje. Na ekranie wagi pojawi sie pasek postepu. Nacisnij OK aby instalowac.", false));
  });

  server_.on("/exit", HTTP_POST, [this]() {
    exitRequested_ = true;
    server_.send(200, "text/html", resultPage("Wyjscie", "Tryb serwisowy zakonczony.", false));
  });

  server_.on("/generate_204", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/gen_204", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/library/test/success.html", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/connecttest.txt", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/ncsi.txt", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.onNotFound([this]() { redirectToPortal(); });
}

void ServicePortal::tick() {
  if (!running_) {
    return;
  }

  dnsServer_.processNextRequest();
  server_.handleClient();
}

void ServicePortal::stop() {
  server_.stop();
  dnsServer_.stop();
  running_ = false;
  WiFi.softAPdisconnect(true);
}

bool ServicePortal::isRunning() const { return running_; }

bool ServicePortal::exitRequested() const { return exitRequested_; }

bool ServicePortal::otaRequested() const { return otaRequested_; }

void ServicePortal::clearExitRequest() { exitRequested_ = false; }

void ServicePortal::clearOtaRequest() { otaRequested_ = false; }

void ServicePortal::handleCaptiveProbe() { redirectToPortal(); }

void ServicePortal::redirectToPortal() {
  const String redirectUrl = "http://" + WiFi.softAPIP().toString() + "/";
  server_.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server_.sendHeader("Pragma", "no-cache");
  server_.sendHeader("Expires", "-1");
  server_.sendHeader("Location", redirectUrl, true);
  server_.send(302, "text/plain", "");
}

String ServicePortal::resultPage(const char* title, const char* message, bool backLink) const {
  String html;
  html.reserve(512);
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>";
  html += title;
  html += "</title></head><body><h2>";
  html += title;
  html += "</h2><p>";
  html += message;
  html += "</p>";
  if (backLink) {
    html += "<p><a href='/'>Powrot</a></p>";
  }
  html += "</body></html>";
  return html;
}

String ServicePortal::buildPage() const {
  const int defaultCal = actions_.defaultCalWeightGrams();

  String html;
  html.reserve(8192);
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>WagaWezy — serwis</title>";
  html += "<style>body{font-family:sans-serif;margin:16px}section{margin:20px 0;padding:12px 0;border-top:1px solid #ccc}";
  html += "button,input[type=submit]{padding:10px 16px;margin:4px 0}input[type=number]{width:120px;padding:8px}";
  html += "input[type=text]{width:100%;max-width:280px;padding:8px;box-sizing:border-box}";
  html += ".btn-row{margin:12px 0;padding:8px 0;border-bottom:1px solid #eee}</style>";
  html += "</head><body>";
  html += "<h2>Tryb serwisowy</h2>";
  const String apPin = appPrefs_.loadApPin();

  html += "<p>AP: <b>";
  html += apName_;
  html += "</b><br>Haslo AP: <b>";
  html += escapeHtmlAttr(apPin);
  html += "</b><br>Adres: <b>";
  html += WiFi.softAPIP().toString();
  html += "</b></p>";

  html += "<section><h3>Haslo punktu dostepowego (AP)</h3>";
  html += "<form method='POST' action='/ap/save'>";
  html += "<p>Haslo do polaczenia z siecia ";
  html += apName_;
  html += " (min. ";
  html += AppPreferences::kMinApPinLen;
  html += " znakow). Domyslnie: ";
  html += AppPreferences::defaultApPin();
  html += ".</p>";
  html += "<input type='text' name='ap_pin' maxlength='";
  html += AppPreferences::kMaxApPinLen;
  html += "' value='";
  html += escapeHtmlAttr(apPin);
  html += "'><br><br>";
  html += "<input type='submit' value='Zapisz haslo AP'></form></section>";

  const WelcomeMessage welcome = appPrefs_.loadWelcomeMessage();

  html += "<section><h3>Ekran powitalny</h3>";
  html += "<form method='POST' action='/welcome/save'>";
  html += "<p>Tekst wyswietlany przy starcie (3 linie, max ";
  html += AppPreferences::kMaxWelcomeLineLen;
  html += " znakow). Domyslnie: Witam / Pszczelarza / z Wachocka.</p>";
  html += "<label>Linia 1:</label><br>";
  html += "<input type='text' name='welcome_l1' maxlength='";
  html += AppPreferences::kMaxWelcomeLineLen;
  html += "' value='";
  html += escapeHtmlAttr(welcome.line1);
  html += "'><br><br>";
  html += "<label>Linia 2:</label><br>";
  html += "<input type='text' name='welcome_l2' maxlength='";
  html += AppPreferences::kMaxWelcomeLineLen;
  html += "' value='";
  html += escapeHtmlAttr(welcome.line2);
  html += "'><br><br>";
  html += "<label>Linia 3:</label><br>";
  html += "<input type='text' name='welcome_l3' maxlength='";
  html += AppPreferences::kMaxWelcomeLineLen;
  html += "' value='";
  html += escapeHtmlAttr(welcome.line3);
  html += "'><br><br>";
  html += "<input type='submit' value='Zapisz ekran powitalny'></form></section>";

  const WifiCredentials savedWifi = appPrefs_.loadWifiCredentials();

  html += "<section><h3>WiFi</h3>";
  if (savedWifi.valid) {
    html += "<p>Zapisana siec: <b>";
    html += escapeHtmlAttr(savedWifi.ssid);
    html += "</b> (haslo zapisane w pamieci)</p>";
  } else {
    html += "<p>Brak zapisanej sieci WiFi.</p>";
  }
  html += "<form method='POST' action='/wifi/save'>";
  html += "<label>Siec:</label><br><select name='ssid'>";
  for (int i = 0; i < networkCount_; ++i) {
    html += "<option value='";
    html += networks_[i];
    html += "'>";
    html += networks_[i];
    html += "</option>";
  }
  html += "</select><br><br>";
  html += "<label>Lub SSID:</label><br><input type='text' name='manual_ssid'><br><br>";
  html += "<label>Haslo:</label><br><input type='password' name='password'><br><br>";
  html += "<input type='submit' value='Zapisz WiFi'></form></section>";

  const String apiEndpoint = appPrefs_.loadApiEndpoint();
  const uint16_t pingInterval = appPrefs_.loadPingIntervalSec();

  html += "<section><h3>Wysylka danych</h3>";
  html += "<form method='POST' action='/endpoint/save'>";
  html += "<p>Adres URL (POST JSON):</p>";
  html += "<input type='text' name='endpoint' maxlength='";
  html += AppPreferences::kMaxEndpointLen;
  html += "' value='";
  html += escapeHtmlAttr(apiEndpoint);
  html += "' placeholder='https://example.com/api/weigh'><br><br>";
  html += "<input type='submit' value='Zapisz endpoint'></form>";
  html += "<form method='POST' action='/ping/save'>";
  html += "<p>Interwal pinga (sekundy, 0 = wyl.). Ping idzie na ten sam URL co pomiar.</p>";
  html += "<input type='number' name='ping_interval' min='0' max='";
  html += AppPreferences::kMaxPingIntervalSec;
  html += "' value='";
  html += pingInterval;
  html += "'><br><br>";
  html += "<input type='submit' value='Zapisz interwal pinga'></form>";
  html += "<form method='POST' action='/endpoint/test'>";
  html += "<p>Test: wysle JSON z aktualna waga i numerem przycisku 1 (wymaga WiFi i RTC).</p>";
  html += "<input type='submit' value='Testuj wysylke'></form></section>";

  html += "<section><h3>Tara</h3>";
  html += "<form method='POST' action='/tara'>";
  html += "<p>Ustaw tare z aktualnego obciazenia (min. 20 g).</p>";
  html += "<input type='submit' value='Zapisz tare'></form></section>";

  html += "<section><h3>Kalibracja</h3>";
  html += "<form method='POST' action='/cal/empty'>";
  html += "<p>Krok 1: opróznij wage i potwierdz.</p>";
  html += "<input type='submit' value='Opróznij wage'></form>";
  html += "<form method='POST' action='/cal/weight'>";
  html += "<p>Krok 2: nałóż znane obciążenie (100–50000 g).</p>";
  html += "<input type='number' name='grams' min='100' max='50000' step='50' value='";
  html += defaultCal;
  html += "'> g<br><br>";
  html += "<input type='submit' value='Kalibruj'></form>";
  html += "<form method='POST' action='/cal/hx711-reset' onsubmit=\"return confirm('Zresetowac modul HX711 do ustawien domyslnych?');\">";
  html += "<p>Reset chipu HX711 (power cycle, skala=1, offset=0) i ponowne zaladowanie zapisanej kalibracji.</p>";
  html += "<input type='submit' value='Kasuj HX711 do default'></form></section>";

  html += "<section><h3>Zegar DS3231 (Polska)</h3>";
  html += "<p>Aktualny czas w module: <b>";
  html += clockActions_.currentTimeText();
  html += "</b></p>";
  html += "<form method='POST' action='/time/ntp'>";
  html += "<p>Pobierz czas z internetu (NTP, Polska CET/CEST z automatycznym czasem letnim). Wymaga zapisanej sieci WiFi.</p>";
  html += "<input type='submit' value='Get time z internetu'></form>";
  html += "<form method='POST' action='/time/set'>";
  html += "<p>Ustaw recznie i zapisz do DS3231:</p>";
  html += "<label>Rok:</label> <input type='number' name='year' min='2020' max='2099' value='2026'><br>";
  html += "<label>Miesiac:</label> <input type='number' name='month' min='1' max='12' value='1'><br>";
  html += "<label>Dzien:</label> <input type='number' name='day' min='1' max='31' value='1'><br>";
  html += "<label>Godzina:</label> <input type='number' name='hour' min='0' max='23' value='12'><br>";
  html += "<label>Minuta:</label> <input type='number' name='minute' min='0' max='59' value='0'><br><br>";
  html += "<input type='submit' value='Zapisz czas recznie'></form></section>";

  html += "<section><h3>Reset</h3>";
  html += "<form method='POST' action='/reset' onsubmit=\"return confirm('Usunac kalibracje i tare?');\">";
  html += "<input type='submit' value='Reset wagi (NVS)'></form></section>";

  html += "<section><h3>Aktualizacja OTA</h3>";
  html += "<form method='POST' action='/ota'>";
  html += "<p>Po starcie trzymaj przycisk OK na wadze aby potwierdzic instalacje.</p>";
  html += "<input type='submit' value='Sprawdz aktualizacje'></form></section>";

  html += "<section><form method='POST' action='/exit'>";
  html += "<input type='submit' value='Wyjdź z trybu serwisowego'></form></section>";

  html += "</body></html>";
  return html;
}

void ServicePortal::restartAp() {
  const String apPin = appPrefs_.loadApPin();
  WiFi.softAPdisconnect(true);
  WiFi.softAP(apName_.c_str(), apPin.c_str());
  dnsServer_.stop();
  dnsServer_.start(53, "*", WiFi.softAPIP());
}

void ServicePortal::scanNetworks() {
  networkCount_ = 0;
  const int n = WiFi.scanNetworks(false, true);
  if (n <= 0) {
    return;
  }

  const int limit = (n < kMaxNetworks) ? n : kMaxNetworks;
  for (int i = 0; i < limit; ++i) {
    networks_[networkCount_++] = WiFi.SSID(i);
  }
  WiFi.scanDelete();
}

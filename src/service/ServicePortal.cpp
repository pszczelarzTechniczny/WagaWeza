#include "ServicePortal.h"

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
    if (wsTestFn_ == nullptr) {
      server_.send(500, "text/html", resultPage("Test", "Niedostepny", true));
      return;
    }
    const String msg = wsTestFn_();
    server_.send(200, "text/html", resultPage("Test WS", msg.c_str(), true));
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

// Strona serwisowa: statyczny markup w PROGMEM, wartości dynamiczne wstrzykiwane
// między fragmentami. Zakładki działają na czystym CSS (radio + :checked), bez JS.
static const char kSvcHead[] PROGMEM = R"html(<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WagaWezy — serwis</title>
<style>
  *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
  body{margin:0;background:#f4f4f5;color:#18181b;
    font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',system-ui,sans-serif;
    font-size:16px;line-height:1.4}
  .app{max-width:520px;margin:0 auto;padding:18px 16px 40px}
  .head{margin-bottom:4px}
  .head h1{font-size:22px;font-weight:750;letter-spacing:-.02em;margin:0}
  .head p{font-size:13.5px;color:#71717a;margin:3px 0 0}

  /* --- zakladki: czysty CSS, bez JS --- */
  input[name=tab]{position:absolute;opacity:0;pointer-events:none}
  .tabsbar{position:sticky;top:0;z-index:5;background:#f4f4f5;padding:10px 0 12px}
  .tabs{display:flex;gap:3px;background:#e4e4e7;border-radius:11px;padding:3px}
  .tabs label{flex:1;min-width:0;text-align:center;height:38px;line-height:38px;
    font-size:13px;font-weight:500;color:#71717a;border-radius:9px;cursor:pointer;
    overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
  #t1:checked~.tabsbar label[for=t1],
  #t2:checked~.tabsbar label[for=t2],
  #t3:checked~.tabsbar label[for=t3],
  #t4:checked~.tabsbar label[for=t4],
  #t5:checked~.tabsbar label[for=t5]{
    background:#fff;color:#18181b;font-weight:600;box-shadow:0 1px 2px rgba(0,0,0,.12)}
  .panel{display:none}
  #t1:checked~.wrap #p1,
  #t2:checked~.wrap #p2,
  #t3:checked~.wrap #p3,
  #t4:checked~.wrap #p4,
  #t5:checked~.wrap #p5{display:block}

  /* --- karty --- */
  .card{background:#fff;border:1px solid #e4e4e7;border-radius:14px;padding:16px;
    margin-bottom:14px;box-shadow:0 1px 2px rgba(0,0,0,.03)}
  .card h2{font-size:15px;font-weight:650;letter-spacing:-.01em;margin:0}
  .note{font-size:13px;line-height:1.45;color:#71717a;margin:6px 0 0}
  form{margin:0}
  .card form+form{margin-top:14px;padding-top:14px;border-top:1px solid #f0f0f1}

  label.fld{display:block;margin-top:14px}
  label.fld>span{display:block;font-size:13px;font-weight:500;color:#3f3f46;margin-bottom:6px}
  input[type=text],input[type=password],input[type=number],select{
    width:100%;height:44px;padding:0 12px;font-size:16px;border:1px solid #d4d4d8;
    border-radius:10px;background:#fff;color:#18181b;outline:none;font-family:inherit}
  input[type=text].mono,input[type=password].mono,input[type=number]{
    font-family:ui-monospace,SFMono-Regular,Menlo,monospace}
  input:focus,select:focus{border-color:#2563eb;box-shadow:0 0 0 3px rgba(37,99,235,.12)}
  select{appearance:none;-webkit-appearance:none;padding-right:36px;
    background-image:url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'><path d='M2 4l4 4 4-4' stroke='%2371717a' stroke-width='1.5' fill='none' stroke-linecap='round' stroke-linejoin='round'/></svg>");
    background-repeat:no-repeat;background-position:right 12px center}
  .num{display:flex;align-items:center;gap:10px}
  .num input{width:150px}
  .num span.suf{font-size:15px;color:#71717a;font-family:ui-monospace,SFMono-Regular,Menlo,monospace}
  .grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:14px}
  .grid label{display:flex;flex-direction:column;gap:5px}
  .grid label span{font-size:12px;color:#71717a}
  .grid input{height:42px;font-size:15px}

  /* --- info wiersze --- */
  .inf{display:flex;align-items:center;justify-content:space-between;gap:12px;
    padding:11px 12px;background:#f4f4f5;border-radius:10px;margin-top:10px}
  .inf .il{font-size:13px;color:#71717a}
  .inf .iv{font-size:14px;font-weight:600;font-family:ui-monospace,SFMono-Regular,Menlo,monospace}

  /* --- przyciski --- */
  .btns{margin-top:14px;display:flex;flex-direction:column;gap:8px}
  button{width:100%;height:46px;border-radius:10px;font-size:15px;font-weight:600;
    cursor:pointer;font-family:inherit;border:1px solid transparent}
  button:active{transform:translateY(1px)}
  .bp{background:#2563eb;color:#fff}
  .bd{background:#fff;border-color:#d4d4d8;color:#18181b}
  .bx{background:#fff;border-color:#fecaca;color:#dc2626}
</style>
</head>
<body>
<div class="app">

  <div class="head">
    <h1>Tryb serwisowy</h1>
    <p>WagaWezy &middot; konfiguracja urz&#261;dzenia</p>
  </div>

  <input type="radio" name="tab" id="t1" checked>
  <input type="radio" name="tab" id="t2">
  <input type="radio" name="tab" id="t3">
  <input type="radio" name="tab" id="t4">
  <input type="radio" name="tab" id="t5">

  <div class="tabsbar">
    <div class="tabs">
      <label for="t1">Sie&#263;</label>
      <label for="t2">Waga</label>
      <label for="t3">Zegar</label>
      <label for="t4">Ekran</label>
      <label for="t5">System</label>
    </div>
  </div>

  <div class="wrap">

    <!-- ====== SIEC ====== -->
    <div class="panel" id="p1">
      <div class="card">
        <h2>Po&#322;&#261;czenie serwisowe</h2>
        <p class="note">Dane dost&#281;powe do trybu konfiguracji.</p>
        <div class="inf"><span class="il">Sie&#263; AP</span><span class="iv">)html";

static const char kSvcApPinRow[] PROGMEM = R"html(</span></div>
        <div class="inf"><span class="il">Has&#322;o AP</span><span class="iv">)html";

static const char kSvcAddrRow[] PROGMEM = R"html(</span></div>
        <div class="inf"><span class="il">Adres</span><span class="iv">)html";

static const char kSvcApPinCardA[] PROGMEM = R"html(</span></div>
      </div>

      <div class="card">
        <h2>Has&#322;o punktu dost&#281;powego</h2>
        <p class="note">Has&#322;o do sieci )html";

static const char kSvcApPinCardB[] PROGMEM = R"html( (min. 8 znak&oacute;w). Domy&#347;lnie 12340000.</p>
        <form method="POST" action="/ap/save">
          <label class="fld"><span>Has&#322;o AP</span>
            <input type="text" class="mono" name="ap_pin" maxlength="63" value=")html";

static const char kSvcWifiCardA[] PROGMEM = R"html("></label>
          <div class="btns"><button class="bp" type="submit">Zapisz has&#322;o AP</button></div>
        </form>
      </div>

      <div class="card">
        <h2>Sie&#263; WiFi</h2>
)html";

static const char kSvcWifiFormA[] PROGMEM = R"html(        <form method="POST" action="/wifi/save">
          <label class="fld"><span>Sie&#263;</span>
            <select name="ssid">)html";

static const char kSvcEndpointA[] PROGMEM = R"html(</select></label>
          <label class="fld"><span>Lub wpisz SSID</span>
            <input type="text" name="manual_ssid" placeholder="nazwa sieci"></label>
          <label class="fld"><span>Has&#322;o</span>
            <input type="password" name="password"></label>
          <div class="btns"><button class="bp" type="submit">Zapisz WiFi</button></div>
        </form>
      </div>

      <div class="card">
        <h2>Serwer / wysy&#322;ka danych</h2>
        <p class="note">Adres URL &mdash; pomiary wysy&#322;ane jako JSON metod&#261; POST.</p>
        <form method="POST" action="/endpoint/save">
          <label class="fld"><span>Endpoint</span>
            <input type="text" class="mono" name="endpoint" maxlength="256" value=")html";

static const char kSvcPingA[] PROGMEM = R"html(" placeholder="https://example.com/api/weigh"></label>
          <div class="btns"><button class="bp" type="submit">Zapisz endpoint</button></div>
        </form>
        <form method="POST" action="/endpoint/test">
          <p class="note">Test po&#322;&#261;czy si&#281; z serwerem WebSocket POS (wymaga WiFi).</p>
          <div class="btns"><button class="bd" type="submit">Testuj wysy&#322;k&#281;</button></div>
        </form>
      </div>

      <div class="card">
        <h2>Interwa&#322; pinga</h2>
        <p class="note">Sekundy (0 = wy&#322;&#261;czony). Ping idzie na ten sam URL co pomiar.</p>
        <form method="POST" action="/ping/save">
          <label class="fld"><span>Interwa&#322;</span>
            <div class="num"><input type="number" name="ping_interval" min="0" max="3600" value=")html";

static const char kSvcScaleA[] PROGMEM = R"html("><span class="suf">s</span></div></label>
          <div class="btns"><button class="bp" type="submit">Zapisz interwa&#322;</button></div>
        </form>
      </div>
    </div>

    <!-- ====== WAGA ====== -->
    <div class="panel" id="p2">
      <div class="card">
        <h2>Tara</h2>
        <p class="note">Ustaw tar&#281; z aktualnego obci&#261;&#380;enia (min. 20 g).</p>
        <form method="POST" action="/tara">
          <div class="btns"><button class="bp" type="submit">Zapisz tar&#281;</button></div>
        </form>
      </div>

      <div class="card">
        <h2>Kalibracja</h2>
        <p class="note">Krok 1: opr&oacute;&#380;nij wag&#281;. Krok 2: na&#322;&oacute;&#380; znane obci&#261;&#380;enie (100&ndash;50000 g) i potwierd&#378;.</p>
        <form method="POST" action="/cal/empty">
          <div class="btns"><button class="bd" type="submit">Opr&oacute;&#380;nij wag&#281;</button></div>
        </form>
        <form method="POST" action="/cal/weight">
          <label class="fld"><span>Znane obci&#261;&#380;enie</span>
            <div class="num"><input type="number" name="grams" min="100" max="50000" step="50" value=")html";

static const char kSvcClockA[] PROGMEM = R"html("><span class="suf">g</span></div></label>
          <div class="btns"><button class="bp" type="submit">Kalibruj</button></div>
        </form>
      </div>

      <div class="card">
        <h2>Reset modu&#322;u HX711</h2>
        <p class="note">Power cycle, skala = 1, offset = 0 i ponowne za&#322;adowanie zapisanej kalibracji.</p>
        <form method="POST" action="/cal/hx711-reset" onsubmit="return confirm('Zresetowa&#263; modu&#322; HX711 do ustawie&#324; domy&#347;lnych?');">
          <div class="btns"><button class="bx" type="submit">Kasuj HX711 do default</button></div>
        </form>
      </div>
    </div>

    <!-- ====== ZEGAR ====== -->
    <div class="panel" id="p3">
      <div class="card">
        <h2>Zegar DS3231 (Polska)</h2>
        <div class="inf"><span class="il">Czas w module</span><span class="iv">)html";

static const char kSvcWelcomeA[] PROGMEM = R"html(</span></div>
        <p class="note">Pobierz czas z internetu (NTP, CET/CEST, automatyczny czas letni). Wymaga zapisanej sieci WiFi.</p>
        <form method="POST" action="/time/ntp">
          <div class="btns"><button class="bd" type="submit">Pobierz czas z internetu</button></div>
        </form>
      </div>

      <div class="card">
        <h2>Ustaw r&#281;cznie</h2>
        <p class="note">Zapisz wskazany czas do DS3231.</p>
        <form method="POST" action="/time/set">
          <div class="grid">
            <label><span>Rok</span><input type="number" name="year" min="2020" max="2099" value="2026"></label>
            <label><span>Miesi&#261;c</span><input type="number" name="month" min="1" max="12" value="1"></label>
            <label><span>Dzie&#324;</span><input type="number" name="day" min="1" max="31" value="1"></label>
            <label><span>Godzina</span><input type="number" name="hour" min="0" max="23" value="12"></label>
            <label><span>Minuta</span><input type="number" name="minute" min="0" max="59" value="0"></label>
          </div>
          <div class="btns"><button class="bp" type="submit">Zapisz czas r&#281;cznie</button></div>
        </form>
      </div>
    </div>

    <!-- ====== EKRAN ====== -->
    <div class="panel" id="p4">
      <div class="card">
        <h2>Ekran powitalny</h2>
        <p class="note">Tekst wy&#347;wietlany przy starcie (3 linie, max 24 znaki).</p>
        <form method="POST" action="/welcome/save">
          <label class="fld"><span>Linia 1</span><input type="text" name="welcome_l1" maxlength="24" value=")html";

static const char kSvcWelcomeB[] PROGMEM = R"html("></label>
          <label class="fld"><span>Linia 2</span><input type="text" name="welcome_l2" maxlength="24" value=")html";

static const char kSvcWelcomeC[] PROGMEM = R"html("></label>
          <label class="fld"><span>Linia 3</span><input type="text" name="welcome_l3" maxlength="24" value=")html";

static const char kSvcTail[] PROGMEM = R"html("></label>
          <div class="btns"><button class="bp" type="submit">Zapisz ekran powitalny</button></div>
        </form>
      </div>
    </div>

    <!-- ====== SYSTEM ====== -->
    <div class="panel" id="p5">
      <div class="card">
        <h2>Aktualizacja OTA</h2>
        <p class="note">Po starcie trzymaj przycisk OK na wadze, aby potwierdzi&#263; instalacj&#281;.</p>
        <form method="POST" action="/ota">
          <div class="btns"><button class="bd" type="submit">Sprawd&#378; aktualizacje</button></div>
        </form>
      </div>

      <div class="card">
        <h2>Reset wagi</h2>
        <p class="note">Usuwa zapisan&#261; kalibracj&#281; i tar&#281; (NVS).</p>
        <form method="POST" action="/reset" onsubmit="return confirm('Usun&#261;&#263; kalibracj&#281; i tar&#281;?');">
          <div class="btns"><button class="bx" type="submit">Reset wagi</button></div>
        </form>
      </div>

      <div class="card">
        <h2>Wyj&#347;cie</h2>
        <p class="note">Zako&#324;cz tryb serwisowy i uruchom normaln&#261; prac&#281; wagi.</p>
        <form method="POST" action="/exit">
          <div class="btns"><button class="bd" type="submit">Wyjd&#378; z trybu serwisowego</button></div>
        </form>
      </div>
    </div>

  </div>
</div>
</body>
</html>
)html";

String ServicePortal::buildPage() const {
  const int defaultCal = actions_.defaultCalWeightGrams();
  const String apPin = appPrefs_.loadApPin();
  const WelcomeMessage welcome = appPrefs_.loadWelcomeMessage();
  const WifiCredentials savedWifi = appPrefs_.loadWifiCredentials();
  const String apiEndpoint = appPrefs_.loadApiEndpoint();
  const uint16_t pingInterval = appPrefs_.loadPingIntervalSec();

  String html;
  html.reserve(14336);

  html += FPSTR(kSvcHead);
  html += apName_;
  html += FPSTR(kSvcApPinRow);
  html += escapeHtmlAttr(apPin);
  html += FPSTR(kSvcAddrRow);
  html += WiFi.softAPIP().toString();
  html += FPSTR(kSvcApPinCardA);
  html += apName_;
  html += FPSTR(kSvcApPinCardB);
  html += escapeHtmlAttr(apPin);
  html += FPSTR(kSvcWifiCardA);

  if (savedWifi.valid) {
    html += "        <p class=\"note\">Zapisana sie&#263;: <b>";
    html += escapeHtmlAttr(savedWifi.ssid);
    html += "</b> (has&#322;o zapisane w pami&#281;ci). Wybierz z listy lub wpisz SSID, aby zmieni&#263;.</p>\n";
  } else {
    html += "        <p class=\"note\">Brak zapisanej sieci. Wybierz z listy lub wpisz SSID r&#281;cznie.</p>\n";
  }

  html += FPSTR(kSvcWifiFormA);
  for (int i = 0; i < networkCount_; ++i) {
    html += "<option value=\"";
    html += escapeHtmlAttr(networks_[i]);
    html += "\">";
    html += escapeHtmlAttr(networks_[i]);
    html += "</option>";
  }
  html += FPSTR(kSvcEndpointA);
  html += escapeHtmlAttr(apiEndpoint);
  html += FPSTR(kSvcPingA);
  html += pingInterval;
  html += FPSTR(kSvcScaleA);
  html += defaultCal;
  html += FPSTR(kSvcClockA);
  html += clockActions_.currentTimeText();
  html += FPSTR(kSvcWelcomeA);
  html += escapeHtmlAttr(welcome.line1);
  html += FPSTR(kSvcWelcomeB);
  html += escapeHtmlAttr(welcome.line2);
  html += FPSTR(kSvcWelcomeC);
  html += escapeHtmlAttr(welcome.line3);
  html += FPSTR(kSvcTail);

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

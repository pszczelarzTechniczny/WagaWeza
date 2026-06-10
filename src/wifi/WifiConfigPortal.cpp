#include "WifiConfigPortal.h"

WifiConfigPortal::WifiConfigPortal()
    : server_(80),
      running_(false),
      saveReady_(false),
      startedAtMs_(0),
      timeoutMs_(0),
      networkCount_(0) {}

bool WifiConfigPortal::begin(const char* apName, const char* apPassword, uint32_t timeoutMs) {
  stop();

  apName_ = apName;
  apPin_ = apPassword ? apPassword : "";
  timeoutMs_ = timeoutMs;
  startedAtMs_ = millis();
  saveReady_ = false;
  pendingSsid_ = "";
  pendingPassword_ = "";

  WiFi.mode(WIFI_AP_STA);
  const bool apOk = WiFi.softAP(apName_.c_str(), apPin_.c_str());
  if (!apOk) {
    return false;
  }

  scanNetworks();

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/generate_204", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/gen_204", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/library/test/success.html", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/connecttest.txt", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.on("/ncsi.txt", HTTP_GET, [this]() { handleCaptiveProbe(); });
  server_.onNotFound([this]() { redirectToPortal(); });
  server_.begin();

  dnsServer_.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer_.start(53, "*", WiFi.softAPIP());

  running_ = true;
  return true;
}

WifiConfigPortal::Result WifiConfigPortal::tick() {
  if (!running_) {
    return RESULT_NONE;
  }

  dnsServer_.processNextRequest();
  server_.handleClient();

  if (saveReady_) {
    running_ = false;
    return RESULT_SAVED;
  }

  if (timeoutMs_ > 0 && millis() - startedAtMs_ > timeoutMs_) {
    running_ = false;
    return RESULT_TIMEOUT;
  }

  return RESULT_NONE;
}

void WifiConfigPortal::stop() {
  server_.stop();
  dnsServer_.stop();
  running_ = false;
  WiFi.softAPdisconnect(true);
}

bool WifiConfigPortal::isRunning() const { return running_; }

String WifiConfigPortal::savedSsid() const { return pendingSsid_; }

String WifiConfigPortal::savedPassword() const { return pendingPassword_; }

void WifiConfigPortal::handleRoot() { server_.send(200, "text/html", buildPage()); }

void WifiConfigPortal::handleCaptiveProbe() { redirectToPortal(); }

void WifiConfigPortal::handleSave() {
  String ssid = server_.arg("ssid");
  ssid.trim();
  if (ssid.length() == 0) {
    ssid = server_.arg("manual_ssid");
    ssid.trim();
  }
  String password = server_.arg("password");

  if (ssid.length() == 0) {
    server_.send(400, "text/plain", "SSID wymagane");
    return;
  }

  pendingSsid_ = ssid;
  pendingPassword_ = password;
  saveReady_ = true;

  server_.send(200, "text/html",
               "<html><body><h2>Zapisano.</h2><p>ESP32 laczy z WiFi...</p></body></html>");
}

void WifiConfigPortal::redirectToPortal() {
  const String redirectUrl = "http://" + WiFi.softAPIP().toString() + "/";
  server_.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server_.sendHeader("Pragma", "no-cache");
  server_.sendHeader("Expires", "-1");
  server_.sendHeader("Location", redirectUrl, true);
  server_.send(302, "text/plain", "");
}

String WifiConfigPortal::buildPage() const {
  String html;
  html.reserve(4096);
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Konfiguracja WiFi</title></head><body>";
  html += "<h2>Konfiguracja WiFi</h2>";
  html += "<p>AP: ";
  html += apName_;
  html += "<br>Haslo AP: ";
  html += apPin_;
  html += "</p>";
  html += "<p>Jesli strona nie otworzy sie automatycznie, wpisz w przegladarce: ";
  html += WiFi.softAPIP().toString();
  html += "</p>";
  html += "<form method='POST' action='/save'>";
  html += "<label>Siec:</label><br><select name='ssid'>";
  for (int i = 0; i < networkCount_; ++i) {
    html += "<option value='";
    html += networks_[i];
    html += "'>";
    html += networks_[i];
    html += "</option>";
  }
  html += "</select><br><br>";
  html += "<label>Lub wpisz SSID:</label><br><input type='text' name='manual_ssid'><br><br>";
  html += "<label>Haslo:</label><br><input type='password' name='password'><br><br>";
  html += "<button type='submit'>Zapisz</button></form>";
  html += "</body></html>";
  return html;
}

void WifiConfigPortal::scanNetworks() {
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

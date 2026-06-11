#include "PosLink.h"

#include <WiFi.h>

#include "../rtc/RtcClock.h"

PosLink* PosLink::instance_ = nullptr;

PosLink::PosLink()
    : appPrefs_(nullptr),
      rtc_(nullptr),
      bootCount_(0),
      wsDisconnects_(0),
      lastAckRttMs_(0),
      buttonSentAtMs_(0),
      suspended_(false),
      started_(false),
      connected_(false),
      posOnline_(false),
      configValid_(false),
      useTls_(false),
      port_(0),
      eventCounter_(0),
      failedAttempts_(0),
      updateRequested_(false) {}

void PosLink::begin(AppPreferences* appPrefs, const char* fwVersion, RtcClock* rtc) {
  appPrefs_ = appPrefs;
  fwVersion_ = fwVersion != nullptr ? fwVersion : "";
  rtc_ = rtc;
  deviceId_ = WiFi.macAddress();
  instance_ = this;

  char prefix[8];
  snprintf(prefix, sizeof(prefix), "%04x", (unsigned)(esp_random() & 0xFFFF));
  sessionPrefix_ = prefix;

  configValid_ = parseEndpoint(appPrefs_->loadApiEndpoint());
  token_ = appPrefs_->loadWsToken();
  ws_.onEvent(staticEvent);
  ws_.setReconnectInterval(kReconnectFastMs);
  // Heartbeat klienta: wykrywa polaczenie "zombie" (TCP polotwarte po ubiciu
  // sesji przez router/serwer) — bez tego sendWeight blokuje petle do 5 s
  // na kazdej wysylce w martwy socket, a wskaznik S klamie.
  ws_.enableHeartbeat(15000, 3000, 2);
}

bool PosLink::parseEndpoint(const String& endpoint) {
  String rest;
  if (endpoint.startsWith("https://")) {
    useTls_ = true;
    rest = endpoint.substring(8);
  } else if (endpoint.startsWith("http://")) {
    useTls_ = false;
    rest = endpoint.substring(7);
  } else {
    return false;
  }

  int slash = rest.indexOf('/');
  String hostPort = slash >= 0 ? rest.substring(0, slash) : rest;
  const int colon = hostPort.indexOf(':');
  if (colon >= 0) {
    host_ = hostPort.substring(0, colon);
    const long p = hostPort.substring(colon + 1).toInt();
    if (p <= 0 || p > 65535) {
      return false;
    }
    port_ = (uint16_t)p;
  } else {
    host_ = hostPort;
    port_ = useTls_ ? 443 : 80;
  }
  return host_.length() > 0;
}

void PosLink::suspend() {
  suspended_ = true;
  started_ = false;
  connected_ = false;
  posOnline_ = false;
  failedAttempts_ = 0;
  ws_.disconnect();
}

void PosLink::resume() {
  suspended_ = false;
  failedAttempts_ = 0;
  ws_.setReconnectInterval(kReconnectFastMs);
  configValid_ = parseEndpoint(appPrefs_->loadApiEndpoint());
  token_ = appPrefs_->loadWsToken();
}

String PosLink::wsPath() const {
  String path = "/ws/scale";
  if (token_.length() > 0) {
    path += "?token=";
    path += urlEncode(token_);
  }
  return path;
}

void PosLink::connectIfNeeded() {
  if (started_ || suspended_ || !configValid_) {
    return;
  }
  started_ = true;
  const String path = wsPath();
  if (useTls_) {
    ws_.beginSSL(host_.c_str(), port_, path.c_str());
  } else {
    ws_.begin(host_.c_str(), port_, path.c_str());
  }
}

void PosLink::tick() {
  if (suspended_) {
    return;
  }
  // Bez WiFi kazda proba TCP i tak konczy sie timeoutem (blokujacym petle) —
  // czekamy, az WifiConnectionManager przywroci siec.
  if (WiFi.status() != WL_CONNECTED) {
    if (connected_) {
      connected_ = false;
      posOnline_ = false;
      ++wsDisconnects_;
      Serial.println("[ws] rozlaczono (brak WiFi)");
    }
    return;
  }
  connectIfNeeded();
  ws_.loop();
}

bool PosLink::isConnected() const { return connected_; }

bool PosLink::posOnline() const { return connected_ && posOnline_; }

bool PosLink::configValid() const { return configValid_; }

String PosLink::wsUrlText() const {
  if (!configValid_) {
    return "(brak)";
  }
  String url = useTls_ ? "wss://" : "ws://";
  url += host_;
  url += ":";
  url += port_;
  url += "/ws/scale";
  return url;
}

void PosLink::staticEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (instance_ != nullptr) {
    instance_->onEvent(type, payload, length);
  }
}

void PosLink::onEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      connected_ = true;
      failedAttempts_ = 0;
      ws_.setReconnectInterval(kReconnectFastMs);
      Serial.println("[ws] polaczono z POS");
      String hello = "{\"type\":\"hello\",\"role\":\"scale\",\"fw\":\"";
      hello += fwVersion_;
      hello += "\",\"deviceId\":\"";
      hello += deviceId_;
      hello += "\"}";
      sendJson(hello);
      break;
    }
    case WStype_DISCONNECTED:
      if (connected_) {
        Serial.println("[ws] rozlaczono");
        ++wsDisconnects_;
        failedAttempts_ = 0;
      } else if (!suspended_) {
        // Kolejna nieudana proba polaczenia — kazda blokuje petle do 5 s,
        // wiec po serii porazek przechodzimy na rzadszy reconnect.
        ++failedAttempts_;
        if (failedAttempts_ == kBackoffAfterFails) {
          ws_.setReconnectInterval(kReconnectSlowMs);
          Serial.println("[ws] backoff reconnectu (30 s)");
        }
      }
      connected_ = false;
      posOnline_ = false;
      break;
    case WStype_TEXT:
      // Biblioteka null-terminuje payload ramek tekstowych.
      handleText(String((char*)payload));
      break;
    default:
      break;
  }
}

void PosLink::handleText(const String& json) {
  String type;
  if (!jsonFindString(json, "type", type)) {
    return;
  }
  if (type == "pos") {
    bool online = false;
    if (jsonFindBool(json, "online", online)) {
      posOnline_ = online;
      Serial.print("[ws] POS ");
      Serial.println(online ? "online" : "offline");
    }
  } else if (type == "ack") {
    lastAckRttMs_ = millis() - buttonSentAtMs_;
    lastAck_.received = true;
    jsonFindBool(json, "ok", lastAck_.ok);
    lastAck_.error = "";
    jsonFindString(json, "error", lastAck_.error);
    lastAck_.eventId = "";
    jsonFindString(json, "eventId", lastAck_.eventId);
    Serial.print("[ws] ack ");
    Serial.print(lastAck_.eventId);
    Serial.println(lastAck_.ok ? String(" ok") : (" blad: " + lastAck_.error));
  } else if (type == "update") {
    // Gdy waga ma ustawiony token, komenda update musi go zawierac —
    // samo polaczenie nie uwierzytelnia serwera wobec wagi.
    String msgToken;
    jsonFindString(json, "token", msgToken);
    if (token_.length() > 0 && msgToken != token_) {
      Serial.println("[ws] zadanie aktualizacji ODRZUCONE (brak/zly token)");
    } else {
      updateRequested_ = true;
      Serial.println("[ws] zadanie aktualizacji z POS");
    }
  } else if (type == "welcome") {
    Serial.println("[ws] welcome");
  }
}

bool PosLink::takeUpdateRequest() {
  if (!updateRequested_) {
    return false;
  }
  updateRequested_ = false;
  return true;
}

bool PosLink::takeAck(PosAck& out) {
  if (!lastAck_.received) {
    return false;
  }
  out = lastAck_;
  lastAck_ = PosAck();
  return true;
}

void PosLink::sendJson(const String& json) {
  if (!connected_) {
    return;
  }
  ws_.sendTXT(json.c_str());
}

String PosLink::urlEncode(const String& value) {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}

String PosLink::formatKg(float kg) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.3f", kg);
  return String(buf);
}

void PosLink::sendWeight(float kg, bool stable) {
  String json = "{\"type\":\"weight\",\"kg\":";
  json += formatKg(kg);
  json += ",\"stable\":";
  json += stable ? "true" : "false";
  json += "}";
  sendJson(json);
}

String PosLink::buttonJson(const String& eventId, uint8_t slot, float kg) const {
  String json = "{\"type\":\"button\",\"slot\":";
  json += slot;
  json += ",\"kg\":";
  json += formatKg(kg);
  json += ",\"eventId\":\"";
  json += eventId;
  json += "\",\"deviceId\":\"";
  json += deviceId_;
  json += "\"";
  if (rtc_ != nullptr) {
    RtcDateTime dt;
    if (rtc_->read(dt) && RtcClock::isValid(dt)) {
      json += ",\"ts\":\"";
      json += RtcClock::formatIso8601(dt);
      json += "\"";
    }
  }
  json += "}";
  return json;
}

String PosLink::sendButton(uint8_t slot, float kg) {
  if (!connected_) {
    return "";
  }
  ++eventCounter_;
  String eventId = sessionPrefix_;
  eventId += "-";
  eventId += eventCounter_;
  lastAck_ = PosAck();  // nowe zdarzenie uniewaznia zalegly ack
  buttonSentAtMs_ = millis();
  sendJson(buttonJson(eventId, slot, kg));
  return eventId;
}

void PosLink::resendButton(const String& eventId, uint8_t slot, float kg) {
  buttonSentAtMs_ = millis();
  sendJson(buttonJson(eventId, slot, kg));
}

void PosLink::sendUndo() { sendJson("{\"type\":\"undo\"}"); }

void PosLink::setDiagnostics(const char* resetReason, uint32_t bootCount) {
  resetReason_ = resetReason != nullptr ? resetReason : "";
  bootCount_ = bootCount;
}

void PosLink::sendPing(const char* deviceId, const char* ip, const char* ssid, int rssi,
                       unsigned long uptimeSec, unsigned long freeHeap) {
  String json = "{\"type\":\"ping\",\"deviceId\":\"";
  json += deviceId;
  json += "\",\"fw\":\"";
  json += fwVersion_;
  json += "\",\"ip\":\"";
  json += ip;
  json += "\",\"ssid\":\"";
  json += ssid;
  json += "\",\"rssi\":";
  json += rssi;
  json += ",\"uptimeSec\":";
  json += uptimeSec;
  json += ",\"freeHeap\":";
  json += freeHeap;
  json += ",\"resetReason\":\"";
  json += resetReason_;
  json += "\",\"bootCount\":";
  json += bootCount_;
  json += ",\"wsDisconnects\":";
  json += wsDisconnects_;
  json += ",\"ackRttMs\":";
  json += lastAckRttMs_;
  json += "}";
  sendJson(json);
}

// Proste wyszukiwanie pól w płaskim JSON — komunikaty serwera są małe i znane,
// pełny parser nie jest potrzebny.
bool PosLink::jsonFindString(const String& s, const char* key, String& out) {
  String pat = "\"";
  pat += key;
  pat += "\":\"";
  const int start = s.indexOf(pat);
  if (start < 0) {
    return false;
  }
  const int from = start + pat.length();
  const int end = s.indexOf('"', from);
  if (end < 0) {
    return false;
  }
  out = s.substring(from, end);
  return true;
}

bool PosLink::jsonFindBool(const String& s, const char* key, bool& out) {
  String pat = "\"";
  pat += key;
  pat += "\":";
  const int start = s.indexOf(pat);
  if (start < 0) {
    return false;
  }
  const int from = start + pat.length();
  if (s.startsWith("true", from)) {
    out = true;
    return true;
  }
  if (s.startsWith("false", from)) {
    out = false;
    return true;
  }
  return false;
}

#ifndef POS_LINK_H
#define POS_LINK_H

#include <Arduino.h>
#include <WebSocketsClient.h>

#include "../storage/AppPreferences.h"

// Wynik ack serwera POS dla zdarzenia button.
struct PosAck {
  bool received = false;
  bool ok = false;
  String error;
  String eventId;
};

// Klient WebSocket do POSeidona (/ws/scale). URL wyprowadzany z zapisanego
// endpointu HTTP: http(s)://host[:port]/cokolwiek -> ws(s)://host:port/ws/scale.
class PosLink {
 public:
  PosLink();

  void begin(AppPreferences* appPrefs, const char* fwVersion);
  // Rozłącza i wstrzymuje (tryb serwisowy / OTA).
  void suspend();
  // Wznawia po suspend(); przeładowuje URL z NVS (endpoint mógł się zmienić).
  void resume();
  void tick();

  bool isConnected() const;
  bool posOnline() const;
  bool configValid() const;
  String wsUrlText() const;

  void sendWeight(float kg, bool stable);
  // Zwraca eventId ("" gdy niepołączony).
  String sendButton(uint8_t slot, float kg);
  // Retry z tym samym eventId (serwer deduplikuje).
  void resendButton(const String& eventId, uint8_t slot, float kg);
  void sendUndo();
  void sendPing(const char* deviceId, int rssi, unsigned long uptimeSec,
                unsigned long freeHeap);

  // Jednorazowe pobranie ostatniego ack (zeruje bufor). false = brak nowego.
  bool takeAck(PosAck& out);

  // Jednorazowe pobranie żądania zdalnej aktualizacji ({type:"update"} z POS).
  bool takeUpdateRequest();

 private:
  void connectIfNeeded();
  bool parseEndpoint(const String& endpoint);
  void onEvent(WStype_t type, uint8_t* payload, size_t length);
  void handleText(const String& json);
  void sendJson(const String& json);
  String buttonJson(const String& eventId, uint8_t slot, float kg) const;
  String wsPath() const;
  static String formatKg(float kg);
  static String urlEncode(const String& value);
  static bool jsonFindString(const String& s, const char* key, String& out);
  static bool jsonFindBool(const String& s, const char* key, bool& out);

  static PosLink* instance_;
  static void staticEvent(WStype_t type, uint8_t* payload, size_t length);

  AppPreferences* appPrefs_;
  String fwVersion_;
  WebSocketsClient ws_;
  bool suspended_;
  bool started_;
  bool connected_;
  bool posOnline_;
  bool configValid_;
  bool useTls_;
  String host_;
  uint16_t port_;
  String token_;
  String sessionPrefix_;
  uint32_t eventCounter_;
  PosAck lastAck_;
  bool updateRequested_;
};

#endif

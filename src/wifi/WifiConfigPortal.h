#ifndef WIFI_CONFIG_PORTAL_H
#define WIFI_CONFIG_PORTAL_H

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

class WifiConfigPortal {
 public:
  enum Result {
    RESULT_NONE = 0,
    RESULT_SAVED = 1,
    RESULT_TIMEOUT = 2
  };

  WifiConfigPortal();

  bool begin(const char* apName, const char* apPassword, uint32_t timeoutMs);
  Result tick();
  void stop();
  bool isRunning() const;

  String savedSsid() const;
  String savedPassword() const;

 private:
  void handleRoot();
  void handleSave();
  void handleCaptiveProbe();
  void redirectToPortal();
  String buildPage() const;
  void scanNetworks();

  DNSServer dnsServer_;
  WebServer server_;
  bool running_;
  bool saveReady_;
  unsigned long startedAtMs_;
  uint32_t timeoutMs_;
  String apName_;
  String apPin_;

  static const int kMaxNetworks = 20;
  String networks_[kMaxNetworks];
  int networkCount_;

  String pendingSsid_;
  String pendingPassword_;
};

#endif

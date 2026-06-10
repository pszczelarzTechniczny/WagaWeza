#ifndef SERVICE_PORTAL_H
#define SERVICE_PORTAL_H

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "../storage/AppPreferences.h"
#include "ClockServiceActions.h"
#include "ScaleServiceActions.h"

class ServicePortal {
 public:
  ServicePortal(ScaleServiceActions& actions, ClockServiceActions& clockActions,
                AppPreferences& appPrefs);

  bool begin(const char* apName);
  void tick();
  void stop();
  bool isRunning() const;

  bool exitRequested() const;
  bool otaRequested() const;
  void clearExitRequest();
  void clearOtaRequest();

 private:
  void registerRoutes();
  void scanNetworks();
  void restartAp();
  void handleCaptiveProbe();
  void redirectToPortal();
  String buildPage() const;
  String resultPage(const char* title, const char* message, bool backLink) const;

  ScaleServiceActions& actions_;
  ClockServiceActions& clockActions_;
  AppPreferences& appPrefs_;

  DNSServer dnsServer_;
  WebServer server_;

  bool running_;
  bool exitRequested_;
  bool otaRequested_;
  String apName_;

  static const int kMaxNetworks = 20;
  String networks_[kMaxNetworks];
  int networkCount_;
};

#endif

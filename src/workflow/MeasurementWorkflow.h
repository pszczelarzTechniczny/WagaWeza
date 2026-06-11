#ifndef MEASUREMENT_WORKFLOW_H
#define MEASUREMENT_WORKFLOW_H

#include <Arduino.h>

#include "../../config/Buttons.h"
#include "../input/InputButtons.h"
#include "../network/PosLink.h"
#include "../scale/Scale.h"
#include "../scale/StabilityTracker.h"
#include "../ui/Display.h"

// Naciśnięcie slotu 1–6: walidacja (WS, stabilność, masa) → button po WS →
// ack w 2 s → 1 retry z tym samym eventId → wynik na OLED + buzzer.
class MeasurementWorkflow {
 public:
  using BuzzerCallback = void (*)(bool success);

  MeasurementWorkflow();

  void begin(PosLink* link, Display* display, BuzzerCallback buzzer);
  bool tick(InputButtons& buttons, Scale& scale, const StabilityTracker& stability);
  bool isActive() const;

 private:
  enum class State : uint8_t { IDLE, WAIT_ACK, SHOW_RESULT };

  static const unsigned long kAckTimeoutMs = 2000;
  static const unsigned long kResultMs = 1500;

  void showResult(const char* line1, const char* line2, bool success);
  static String ackErrorText(const String& code);

  PosLink* link_;
  Display* display_;
  BuzzerCallback buzzer_;

  State state_;
  unsigned long sentAtMs_;
  unsigned long resultUntilMs_;
  bool retried_;
  uint8_t slot_;       // numer slotu 1–6 (do protokołu i komunikatów)
  float kg_;
  String eventId_;
};

#endif

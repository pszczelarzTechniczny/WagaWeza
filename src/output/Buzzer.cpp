#include "Buzzer.h"

void Buzzer::begin(int pin, bool activeLow) {
  pin_ = pin;
  activeLow_ = activeLow;
  offAtMs_ = 0;
  pinMode(pin_, OUTPUT);
  setSoundOn(false);
}

void Buzzer::setSoundOn(bool on) {
  const int level = activeLow_ ? (on ? LOW : HIGH) : (on ? HIGH : LOW);
  digitalWrite(pin_, level);
}

void Buzzer::beep(unsigned long durationMs) {
  if (durationMs == 0) {
    return;
  }
  setSoundOn(true);
  offAtMs_ = millis() + durationMs;
}

void Buzzer::beepBlocking(unsigned long durationMs) {
  if (durationMs == 0) {
    return;
  }
  setSoundOn(true);
  delay(durationMs);
  setSoundOn(false);
  offAtMs_ = 0;
}

void Buzzer::tick() {
  if (offAtMs_ == 0) {
    return;
  }
  if (millis() >= offAtMs_) {
    setSoundOn(false);
    offAtMs_ = 0;
  }
}

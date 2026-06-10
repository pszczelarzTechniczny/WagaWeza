#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

class Buzzer {
 public:
  void begin(int pin, bool activeLow);
  void tick();
  void beep(unsigned long durationMs);
  void beepBlocking(unsigned long durationMs);

 private:
  void setSoundOn(bool on);

  int pin_;
  bool activeLow_;
  unsigned long offAtMs_;
};

#endif

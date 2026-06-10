#ifndef INPUT_BUTTONS_H
#define INPUT_BUTTONS_H

#include <Arduino.h>

#include "../../config/Buttons.h"

class InputButtons {
 public:
  void begin();
  void tick();

  bool wasPressed(uint8_t index) const;
  bool isHeld(uint8_t index) const;
  bool areBothHeld() const;
  int serviceComboHoldPercent() const;
  bool serviceComboTriggered();

 private:
  static const unsigned long kDebounceMs = 35;
  static const unsigned long kServiceComboHoldMs = 5000;

  bool rawPressed_[BTN_COUNT];
  bool stablePressed_[BTN_COUNT];
  bool edgePressed_[BTN_COUNT];
  unsigned long lastChangeMs_[BTN_COUNT];

  unsigned long serviceComboStartMs_;
  bool serviceComboLatched_;
  bool comboSession_;
};

#endif

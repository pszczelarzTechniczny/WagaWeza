#include "InputButtons.h"

#include "../../config/Pins.h"

void InputButtons::begin() {
  for (uint8_t i = 0; i < BTN_COUNT; ++i) {
    pinMode(pinForButtonIndex(i), INPUT_PULLUP);
    rawPressed_[i] = false;
    stablePressed_[i] = false;
    edgePressed_[i] = false;
    lastChangeMs_[i] = 0;
  }
  serviceComboStartMs_ = 0;
  serviceComboLatched_ = false;
  comboSession_ = false;
}

void InputButtons::tick() {
  const unsigned long now = millis();

  for (uint8_t i = 0; i < BTN_COUNT; ++i) {
    edgePressed_[i] = false;
    const bool reading = (digitalRead(pinForButtonIndex(i)) == LOW);

    if (reading != rawPressed_[i]) {
      rawPressed_[i] = reading;
      lastChangeMs_[i] = now;
    }

    if ((now - lastChangeMs_[i]) > kDebounceMs) {
      if (reading && !stablePressed_[i]) {
        edgePressed_[i] = true;
        Serial.printf("Button %u pressed\n", i + 1);
      }
      stablePressed_[i] = reading;
    }
  }

  const bool both = areBothHeld();
  if (both) {
    comboSession_ = true;
    if (serviceComboStartMs_ == 0) {
      serviceComboStartMs_ = now;
    }
    if (!serviceComboLatched_ && (now - serviceComboStartMs_ >= kServiceComboHoldMs)) {
      serviceComboLatched_ = true;
    }
  } else {
    serviceComboStartMs_ = 0;
    if (!stablePressed_[BTN_TARA] && !stablePressed_[BTN_OK]) {
      comboSession_ = false;
    }
  }
}

bool InputButtons::wasPressed(uint8_t index) const {
  if (index >= BTN_COUNT || comboSession_) {
    return false;
  }
  return edgePressed_[index];
}

bool InputButtons::isHeld(uint8_t index) const {
  if (index >= BTN_COUNT) {
    return false;
  }
  return stablePressed_[index];
}

bool InputButtons::areBothHeld() const {
  return stablePressed_[BTN_TARA] && stablePressed_[BTN_OK];
}

int InputButtons::serviceComboHoldPercent() const {
  if (!areBothHeld() || serviceComboStartMs_ == 0) {
    return 0;
  }
  const unsigned long elapsed = millis() - serviceComboStartMs_;
  return (int)constrain((elapsed * 100UL) / kServiceComboHoldMs, 0UL, 100UL);
}

bool InputButtons::serviceComboTriggered() {
  if (serviceComboLatched_) {
    serviceComboLatched_ = false;
    serviceComboStartMs_ = 0;
    comboSession_ = false;
    return true;
  }
  return false;
}

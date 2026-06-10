#ifndef CONFIG_PINS_H
#define CONFIG_PINS_H

#include <Arduino.h>

#include "Buttons.h"

// HX711 (tensometr)
static const int PIN_HX711_DT = 25;
static const int PIN_HX711_SCK = 26;

// I2C: OLED SH1106 + DS3231
static const int PIN_I2C_SDA = 21;
static const int PIN_I2C_SCL = 22;

// Buzzer aktywny 3,3 V (sterowanie stanem wysokim)
static const int PIN_BUZZER = 18;
static const bool PIN_BUZZER_ACTIVE_LOW = false;

// GPIO przycisków w kolejności indeksów z config/Buttons.h
// (Tara, OK, 1–6; wszystkie do GND, INPUT_PULLUP)
static const int kButtonPins[BTN_COUNT] = {13, 14, 27, 32, 33, 15, 4, 5};

inline int pinForButtonIndex(uint8_t index) {
  return kButtonPins[index];
}

#endif

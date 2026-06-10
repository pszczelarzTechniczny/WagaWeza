#ifndef CONFIG_BUTTONS_H
#define CONFIG_BUTTONS_H

#include <Arduino.h>

// Indeksy przycisków (kolejność zgodna z kButtonPins w config/Pins.h)
static const uint8_t BTN_TARA = 0;
static const uint8_t BTN_OK = 1;
static const uint8_t BTN_1 = 2;
static const uint8_t BTN_2 = 3;
static const uint8_t BTN_3 = 4;
static const uint8_t BTN_4 = 5;
static const uint8_t BTN_5 = 6;
static const uint8_t BTN_6 = 7;
static const uint8_t BTN_COUNT = 8;

// Indeks preferencji/wysyłki (0–5) dla przycisków akcji BTN_1–BTN_6
inline uint8_t actionButtonPrefIndex(uint8_t buttonIndex) {
  return buttonIndex - BTN_1;
}

#endif

#ifndef SCALE_H
#define SCALE_H

#include <Arduino.h>
#include <HX711.h>

#include "../storage/ScalePreferences.h"

class Scale {
 public:
  static const int kMaxWeightGrams = 50000;
  static const int kScaleReads = 2;

  // Margines ponad zakres, od którego zgłaszamy przeciążenie.
  static const int kOverloadMarginGrams = 500;
  // Pas wokół zera, w którym działa auto-zero (dryf termiczny/creep).
  static const int kAutoZeroBandGrams = 30;

  bool begin(int dtPin, int sckPin);
  bool isPresent() const;
  bool isCalibrated() const { return present_ && factor_ != 0.0f; }

  // Nieblokujące próbkowanie HX711: pobiera próbkę tylko gdy przetwornik
  // ma gotowe dane (10/80 SPS) i aktualizuje cache odczytu (mediana z 3 +
  // adaptacyjna EMA + auto-zero). Wołać z loop().
  void tick();

  void applyCalibration(const ScaleCalibrationData& data);
  ScaleCalibrationData calibrationData() const;

  int readNetGrams(int taraOffset) const;
  int readRawGrams() const;
  // Masa ponad zakres tensometru (surowy odczyt, przed przycięciem).
  bool isOverload() const;
  // Odczyt wyraźnie ujemny (podniesiona/zdjęta szalka, dryf) — sugeruj tarę.
  bool isUnderRange() const;
  bool bootTareIfEmpty();
  void tare();
  bool calibrateEmpty();
  bool calibrateWithWeight(int knownGrams, ScaleCalibrationData& out);
  bool resetHx711ToDefault();

  int runtimeTara() const { return runtimeTara_; }
  void setRuntimeTara(int grams) { runtimeTara_ = grams; }

 private:
  mutable HX711 hx711_;
  bool present_;
  float factor_;
  long emptyOffset_;
  int calWeightGrams_;
  int savedTara_;
  int runtimeTara_;
  float emaGrams_;
  bool hasSample_;
  float samples_[3];
  uint8_t sampleIdx_;
  uint8_t sampleCount_;
  float autoZeroGrams_;
  unsigned long autoZeroSinceMs_;
  unsigned long lastAutoZeroStepMs_;
};

#endif

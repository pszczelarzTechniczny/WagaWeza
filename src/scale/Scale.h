#ifndef SCALE_H
#define SCALE_H

#include <Arduino.h>
#include <HX711.h>

#include "../storage/ScalePreferences.h"

class Scale {
 public:
  static const int kMaxWeightGrams = 50000;
  static const int kScaleReads = 2;

  bool begin(int dtPin, int sckPin);
  bool isPresent() const;

  void applyCalibration(const ScaleCalibrationData& data);
  ScaleCalibrationData calibrationData() const;

  int readNetGrams(int taraOffset) const;
  int readRawGrams() const;
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
};

#endif

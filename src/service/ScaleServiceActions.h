#ifndef SCALE_SERVICE_ACTIONS_H
#define SCALE_SERVICE_ACTIONS_H

#include <Arduino.h>

#include "../scale/Scale.h"
#include "../storage/ScalePreferences.h"

class ScaleServiceActions {
 public:
  ScaleServiceActions(Scale& scale, ScalePreferences& prefs);

  String saveTara(bool* ok = nullptr);
  String calibrateEmpty(bool* ok = nullptr);
  String calibrateWithWeight(int grams, bool* ok = nullptr);
  String resetHx711Default();
  String resetAll();
  int defaultCalWeightGrams() const;
  int currentNetWeightGrams() const;

 private:
  Scale& scale_;
  ScalePreferences& prefs_;
};

#endif

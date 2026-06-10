#ifndef SCALE_PREFERENCES_H
#define SCALE_PREFERENCES_H

#include <Arduino.h>
#include <Preferences.h>

struct ScaleCalibrationData {
  float factor;
  long emptyOffset;
  int calWeightGrams;
  int savedTara;
};

class ScalePreferences {
 public:
  bool load(ScaleCalibrationData& out) const;
  bool save(const ScaleCalibrationData& data);
  bool clearAll();

 private:
  static const char* kNamespace;
};

#endif

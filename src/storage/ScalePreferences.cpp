#include "ScalePreferences.h"

#include "../scale/Scale.h"

const char* ScalePreferences::kNamespace = "WEZY";

bool ScalePreferences::load(ScaleCalibrationData& out) const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    out.factor = 0.0f;
    out.emptyOffset = 0;
    out.calWeightGrams = 5000;
    out.savedTara = 0;
    return false;
  }

  out.factor = prefs.getFloat("faktor", 0.0f);
  out.emptyOffset = prefs.getLong("gewicht_leer", 0);
  out.calWeightGrams = prefs.getInt("kali_gewicht", 5000);
  out.savedTara = prefs.getInt("tara_saved", 0);
  prefs.end();

  out.calWeightGrams = constrain(out.calWeightGrams, 100, Scale::kMaxWeightGrams);
  return true;
}

bool ScalePreferences::save(const ScaleCalibrationData& data) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }

  prefs.putFloat("faktor", data.factor);
  prefs.putLong("gewicht_leer", data.emptyOffset);
  prefs.putInt("kali_gewicht", data.calWeightGrams);
  prefs.putInt("tara_saved", data.savedTara);
  prefs.end();
  return true;
}

bool ScalePreferences::clearAll() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  prefs.clear();
  prefs.end();
  return true;
}

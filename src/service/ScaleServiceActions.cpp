#include "ScaleServiceActions.h"

ScaleServiceActions::ScaleServiceActions(Scale& scale, ScalePreferences& prefs)
    : scale_(scale), prefs_(prefs) {}

int ScaleServiceActions::defaultCalWeightGrams() const {
  ScaleCalibrationData data;
  prefs_.load(data);
  return data.calWeightGrams;
}

int ScaleServiceActions::currentNetWeightGrams() const {
  return scale_.readNetGrams(scale_.runtimeTara());
}

String ScaleServiceActions::saveTara(bool* ok) {
  if (ok != nullptr) {
    *ok = false;
  }
  const int raw = scale_.readRawGrams();
  if (raw <= 20) {
    return "Za malo obciazenia (min. 20 g)";
  }

  scale_.setRuntimeTara(raw);
  ScaleCalibrationData data = scale_.calibrationData();
  data.savedTara = raw;
  if (!prefs_.save(data)) {
    return "Blad zapisu tary";
  }
  scale_.applyCalibration(data);
  if (ok != nullptr) {
    *ok = true;
  }
  return "Tara zapisana";
}

String ScaleServiceActions::calibrateEmpty(bool* ok) {
  if (!scale_.calibrateEmpty()) {
    if (ok != nullptr) {
      *ok = false;
    }
    return "Brak wagi (HX711)";
  }
  if (ok != nullptr) {
    *ok = true;
  }
  return "Waga oprózniona — nałóż obciążenie wzorcowe";
}

String ScaleServiceActions::calibrateWithWeight(int grams, bool* ok) {
  if (ok != nullptr) {
    *ok = false;
  }
  grams = constrain(grams, 100, Scale::kMaxWeightGrams);

  ScaleCalibrationData data = scale_.calibrationData();
  data.calWeightGrams = grams;
  Serial.print("[CAL] start kalibracji, poprzednia savedTara=");
  Serial.println(data.savedTara);

  if (!scale_.calibrateWithWeight(grams, data)) {
    return "Blad kalibracji";
  }
  if (!prefs_.save(data)) {
    return "Blad zapisu kalibracji";
  }
  scale_.applyCalibration(data);

  const int raw = scale_.readRawGrams();
  const int net = scale_.readNetGrams(scale_.runtimeTara());
  Serial.print("[CAL] po zapisie NVS: raw=");
  Serial.print(raw);
  Serial.print(" g net=");
  Serial.print(net);
  Serial.println(" g");

  if (ok != nullptr) {
    *ok = true;
  }
  return "Kalibracja OK";
}

String ScaleServiceActions::resetHx711Default() {
  if (!scale_.resetHx711ToDefault()) {
    return "Brak wagi (HX711)";
  }

  ScaleCalibrationData data = scale_.calibrationData();
  scale_.applyCalibration(data);
  return "HX711 zresetowany — ponownie zaladowano kalibracje z pamieci";
}

String ScaleServiceActions::resetAll() {
  if (!prefs_.clearAll()) {
    return "Blad resetu NVS";
  }

  ScaleCalibrationData cleared;
  cleared.factor = 0.0f;
  cleared.emptyOffset = 0;
  cleared.calWeightGrams = 5000;
  cleared.savedTara = 0;
  scale_.applyCalibration(cleared);
  scale_.setRuntimeTara(0);
  return "Zresetowano";
}

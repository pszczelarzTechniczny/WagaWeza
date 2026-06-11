#include "Scale.h"

#include <math.h>

namespace {

void logScaleState(const char* label, HX711& hx711, float factor, long offset, int savedTara,
                   int runtimeTara) {
  Serial.print("[CAL] ");
  Serial.print(label);
  Serial.print(" | ADC=");
  Serial.print(hx711.read());
  Serial.print(" offset=");
  Serial.print(offset);
  Serial.print(" scale=");
  Serial.print(factor, 6);
  Serial.print(" units(5)=");
  Serial.print(hx711.get_units(5), 2);
  Serial.print(" raw_g=");
  Serial.print((int)round(hx711.get_units(2)));
  Serial.print(" savedTara=");
  Serial.print(savedTara);
  Serial.print(" runtimeTara=");
  Serial.println(runtimeTara);
}

}  // namespace

bool Scale::begin(int dtPin, int sckPin) {
  present_ = false;
  factor_ = 0.0f;
  emptyOffset_ = 0;
  calWeightGrams_ = 5000;
  savedTara_ = 0;
  runtimeTara_ = 0;
  emaGrams_ = 0.0f;
  hasSample_ = false;
  sampleIdx_ = 0;
  sampleCount_ = 0;
  autoZeroGrams_ = 0.0f;
  autoZeroSinceMs_ = 0;
  lastAutoZeroStepMs_ = 0;

  hx711_.begin(dtPin, sckPin);
  if (hx711_.wait_ready_timeout(1000)) {
    hx711_.power_up();
    present_ = true;
  }
  return present_;
}

bool Scale::isPresent() const { return present_; }

void Scale::applyCalibration(const ScaleCalibrationData& data) {
  factor_ = data.factor;
  emptyOffset_ = data.emptyOffset;
  calWeightGrams_ = data.calWeightGrams;
  savedTara_ = data.savedTara;
  runtimeTara_ = data.savedTara;
  autoZeroGrams_ = 0.0f;
  autoZeroSinceMs_ = 0;

  if (present_ && factor_ != 0.0f) {
    hx711_.set_scale(factor_);
    hx711_.set_offset(emptyOffset_);
  }
}

ScaleCalibrationData Scale::calibrationData() const {
  ScaleCalibrationData data;
  data.factor = factor_;
  data.emptyOffset = emptyOffset_;
  data.calWeightGrams = calWeightGrams_;
  data.savedTara = savedTara_;
  return data;
}

namespace {

float median3(float a, float b, float c) {
  if (a > b) { const float t = a; a = b; b = t; }
  if (b > c) { const float t = b; b = c; c = t; }
  if (a > b) { const float t = a; a = b; b = t; }
  return b;
}

}  // namespace

void Scale::tick() {
  if (!present_ || !hx711_.is_ready()) {
    return;
  }
  const float grams = hx711_.get_units(1);

  samples_[sampleIdx_] = grams;
  sampleIdx_ = (uint8_t)((sampleIdx_ + 1) % 3);
  if (sampleCount_ < 3) {
    ++sampleCount_;
  }

  // Mediana z 3 próbek odcina pojedyncze skoki HX711.
  const float filtered =
      (sampleCount_ < 3) ? grams : median3(samples_[0], samples_[1], samples_[2]);

  if (!hasSample_) {
    emaGrams_ = filtered;
    hasSample_ = true;
  } else {
    // Adaptacyjna EMA: mocne wygładzenie przy stabilnym odczycie, szybkie
    // nadążanie przy realnej zmianie masy.
    const float alpha = (fabsf(filtered - emaGrams_) > 50.0f) ? 0.6f : 0.25f;
    emaGrams_ = (1.0f - alpha) * emaGrams_ + alpha * filtered;
  }

  // Auto-zero: gdy odczyt długo siedzi w wąskim pasie wokół zera, powoli
  // dociągamy korektę (dryf termiczny/creep) — bez ręcznego tarowania.
  const unsigned long now = millis();
  const float nearZero = emaGrams_ - autoZeroGrams_;
  if (fabsf(nearZero) < (float)kAutoZeroBandGrams && fabsf(nearZero) > 0.5f) {
    if (autoZeroSinceMs_ == 0) {
      autoZeroSinceMs_ = now;
    }
    if (now - autoZeroSinceMs_ >= 5000 && now - lastAutoZeroStepMs_ >= 1000) {
      lastAutoZeroStepMs_ = now;
      autoZeroGrams_ += (nearZero > 0) ? 0.5f : -0.5f;
    }
  } else {
    autoZeroSinceMs_ = 0;
  }
}

int Scale::readRawGrams() const {
  if (!present_ || !hasSample_) {
    return 0;
  }
  const int grams = (int)round(emaGrams_ - autoZeroGrams_);
  return constrain(grams, 0, kMaxWeightGrams);
}

bool Scale::isOverload() const {
  return present_ && hasSample_ &&
         (emaGrams_ - autoZeroGrams_) > (float)(kMaxWeightGrams + kOverloadMarginGrams);
}

bool Scale::isUnderRange() const {
  return present_ && hasSample_ && (emaGrams_ - autoZeroGrams_) < -50.0f;
}

int Scale::readNetGrams(int taraOffset) const {
  const int net = readRawGrams() - taraOffset;
  return constrain(net, 0, kMaxWeightGrams);
}

bool Scale::bootTareIfEmpty() {
  if (!present_ || factor_ == 0.0f) {
    return false;
  }
  // W setup() cache jeszcze pusty — tu odczyt blokujący jest w porządku.
  const int grams = (int)round(hx711_.get_units(kScaleReads));
  if (grams > -20 && grams < 20) {
    tare();
    return true;
  }
  return false;
}

void Scale::tare() {
  if (!present_) {
    return;
  }
  hx711_.tare(kScaleReads * 5);
  emptyOffset_ = hx711_.get_offset();
}

bool Scale::calibrateEmpty() {
  if (!present_) {
    Serial.println("[CAL] calibrateEmpty: brak HX711");
    return false;
  }

  Serial.println("[CAL] === Krok 1: opróznij wage ===");
  logScaleState("przed", hx711_, factor_, emptyOffset_, savedTara_, runtimeTara_);

  hx711_.set_scale();
  hx711_.tare(10);
  delay(300);
  emptyOffset_ = hx711_.get_offset();
  savedTara_ = 0;
  runtimeTara_ = 0;

  logScaleState("po tare", hx711_, 1.0f, emptyOffset_, savedTara_, runtimeTara_);
  Serial.println("[CAL] Tara wyzerowana — ekran pokazuje surowy odczyt (net=raw)");
  return true;
}

bool Scale::resetHx711ToDefault() {
  if (!present_) {
    return false;
  }

  hx711_.power_down();
  delay(100);
  hx711_.power_up();
  if (!hx711_.wait_ready_timeout(1000)) {
    present_ = false;
    return false;
  }

  hx711_.set_scale();
  hx711_.set_offset(0);
  return true;
}

bool Scale::calibrateWithWeight(int knownGrams, ScaleCalibrationData& out) {
  if (!present_ || knownGrams < 100 || knownGrams > kMaxWeightGrams) {
    Serial.print("[CAL] calibrateWithWeight: odrzucono, present=");
    Serial.print(present_);
    Serial.print(" knownGrams=");
    Serial.println(knownGrams);
    return false;
  }

  Serial.println("[CAL] === Krok 2: obciazenie wzorcowe ===");
  Serial.print("[CAL] znana masa=");
  Serial.print(knownGrams);
  Serial.println(" g");
  logScaleState("przed", hx711_, factor_, emptyOffset_, savedTara_, runtimeTara_);

  const float raw = hx711_.get_units(10);
  Serial.print("[CAL] odczyt units(10)=");
  Serial.println(raw, 2);

  if (raw == 0.0f) {
    Serial.println("[CAL] BLAD: odczyt = 0 — wykonaj najpierw krok 1 (opróznij wage)");
    return false;
  }

  factor_ = raw / (float)knownGrams;
  hx711_.set_scale(factor_);
  emptyOffset_ = hx711_.get_offset();
  calWeightGrams_ = knownGrams;
  savedTara_ = 0;
  runtimeTara_ = 0;

  Serial.print("[CAL] factor=");
  Serial.println(factor_, 8);
  logScaleState("po kalibracji", hx711_, factor_, emptyOffset_, savedTara_, runtimeTara_);

  const int verifyRaw = readRawGrams();
  const int verifyNet = readNetGrams(runtimeTara_);
  Serial.print("[CAL] weryfikacja: raw=");
  Serial.print(verifyRaw);
  Serial.print(" g, net=");
  Serial.print(verifyNet);
  Serial.print(" g (oczekiwane ~");
  Serial.print(knownGrams);
  Serial.println(" g)");

  out.factor = factor_;
  out.emptyOffset = emptyOffset_;
  out.calWeightGrams = calWeightGrams_;
  out.savedTara = 0;
  return true;
}

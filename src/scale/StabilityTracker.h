#ifndef STABILITY_TRACKER_H
#define STABILITY_TRACKER_H

#include <Arduino.h>

// Odczyt jest stabilny, gdy przez kWindowMs nie odchylił się o więcej niż
// kThresholdGrams od wartości odniesienia (spec 6.3). Próg 10 g — szum
// tensometru 50 kg przekracza spec'owe ±2 g i stabilność nigdy nie zapadała;
// 10 g odpowiada rozdzielczości wyświetlacza (0,01 kg).
class StabilityTracker {
 public:
  static const int kThresholdGrams = 10;
  static const unsigned long kWindowMs = 600;

  StabilityTracker() : refGrams_(0), refSinceMs_(0), hasRef_(false) {}

  void feed(int grams, unsigned long nowMs) {
    if (!hasRef_ || abs(grams - refGrams_) > kThresholdGrams) {
      refGrams_ = grams;
      refSinceMs_ = nowMs;
      hasRef_ = true;
    }
  }

  bool isStable(unsigned long nowMs) const {
    return hasRef_ && (nowMs - refSinceMs_ >= kWindowMs);
  }

 private:
  int refGrams_;
  unsigned long refSinceMs_;
  bool hasRef_;
};

#endif

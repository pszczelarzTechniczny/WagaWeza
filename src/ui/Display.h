#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>

#include "../storage/AppPreferences.h"

class Display {
 public:
  Display();

  bool begin();
  void showLine(const char* line);
  void showTwoLines(const char* line1, const char* line2);
  void showThreeLinesCentered(const char* line1, const char* line2, const char* line3);
  void showThreeLinesLeft(const char* line1, const char* line2, const char* line3,
                          int progressPercent = -1);
  void showWeight(int grams, const char* clockLine = nullptr);
  void showStatus(const char* line1, const char* line2, int progressPercent = -1);
  void showThreeLinesWithProgress(const char* line1, const char* line2, const char* line3,
                                  int progressPercent);
  void showWelcome(const WelcomeMessage& message);
  void showTimeMenuRoot(const char* dateLine, const char* timeLine);
  void showServiceDateTime(const char* header, const char* dateLine, const char* timeLine);
  void showTimeManualEdit(const char* fieldLabel, int value);

  U8G2& u8g2();

 private:
  U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2_;
};

#endif

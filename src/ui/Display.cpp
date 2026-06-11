#include "Display.h"

#include <Wire.h>

#include "../../config/Pins.h"

Display::Display()
    : u8g2_(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA) {}

bool Display::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  u8g2_.setBusClock(400000);
  if (!u8g2_.begin()) {
    return false;
  }
  u8g2_.enableUTF8Print();
  return true;
}

U8G2& Display::u8g2() { return u8g2_; }

void Display::showLine(const char* line) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_courB12_tf);
  u8g2_.setCursor(0, 32);
  u8g2_.print(line);
  u8g2_.sendBuffer();
}

void Display::showTwoLines(const char* line1, const char* line2) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_courB10_tf);
  u8g2_.setCursor(0, 18);
  u8g2_.print(line1);
  u8g2_.setCursor(0, 40);
  u8g2_.print(line2);
  u8g2_.sendBuffer();
}

void Display::showWeight(int grams, const char* clockLine, const WeightStatus* status) {
  char buf[16];
  const float kg = grams / 1000.0f;
  snprintf(buf, sizeof(buf), "%.2fkg", kg);
  for (char* p = buf; *p != '\0'; ++p) {
    if (*p == '.') {
      *p = ',';
      break;
    }
  }

  static const uint8_t* kWeightFonts[] = {
      u8g2_font_courB24_tf,
      u8g2_font_courB18_tf,
      u8g2_font_courB14_tf,
      u8g2_font_courB12_tf,
  };
  static const int kWeightBaselines[] = {44, 42, 40, 38};

  u8g2_.clearBuffer();

  const uint8_t* font = u8g2_font_courB12_tf;
  int baseline = 38;
  for (int i = 0; i < 4; ++i) {
    u8g2_.setFont(kWeightFonts[i]);
    if (u8g2_.getStrWidth(buf) <= 124) {
      font = kWeightFonts[i];
      baseline = kWeightBaselines[i];
      break;
    }
  }

  u8g2_.setFont(font);
  const int w = u8g2_.getStrWidth(buf);
  u8g2_.setCursor((128 - w) / 2, baseline);
  u8g2_.print(buf);

  if (clockLine != nullptr && clockLine[0] != '\0') {
    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.setCursor(0, 10);
    u8g2_.print(clockLine);
  }

  if (status != nullptr) {
    if (!status->wifi || !status->ws) {
      // Zerwany lancuch polaczenia — wyrazny napis w negatywie zamiast ikon.
      // Rozrozniamy przyczyne: brak sieci vs brak serwera POS.
      u8g2_.drawBox(70, 0, 58, 11);
      u8g2_.setDrawColor(0);
      u8g2_.setFont(u8g2_font_6x10_tf);
      if (!status->wifi) {
        u8g2_.setCursor(72, 9);
        u8g2_.print("BRAK WIFI");
      } else {
        u8g2_.setCursor(73, 9);
        u8g2_.print("OFFLINE");
      }
      u8g2_.setDrawColor(1);
    } else {
      // Gorny prawy rog: trzy kwadraciki W/S/P (pelny = polaczone).
      u8g2_.setFont(u8g2_font_6x10_tf);
      const char labels[3] = {'W', 'S', 'P'};
      const bool states[3] = {status->wifi, status->ws, status->pos};
      int x = 128 - 3 * 12;
      for (int i = 0; i < 3; ++i) {
        if (states[i]) {
          u8g2_.drawBox(x, 1, 9, 9);
          u8g2_.setDrawColor(0);
        } else {
          u8g2_.drawFrame(x, 1, 9, 9);
        }
        u8g2_.setCursor(x + 2, 9);
        u8g2_.print(labels[i]);
        u8g2_.setDrawColor(1);
        x += 12;
      }
    }
    // Dolny lewy rog: kolo stabilnosci (pelne = stabilny odczyt).
    if (status->stable) {
      u8g2_.drawDisc(5, 57, 3);
    } else {
      u8g2_.drawCircle(5, 57, 3);
    }
  }

  u8g2_.sendBuffer();
}

void Display::showStatus(const char* line1, const char* line2, int progressPercent) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_courB10_tf);
  u8g2_.setCursor(0, 18);
  u8g2_.print(line1);
  u8g2_.setCursor(0, 36);
  u8g2_.print(line2);

  if (progressPercent >= 0) {
    const int clamped = constrain(progressPercent, 0, 100);
    u8g2_.drawFrame(0, 48, 128, 14);
    const int bar = (124 * clamped) / 100;
    u8g2_.drawBox(2, 50, bar, 10);
    u8g2_.setCursor(94, 62);
    u8g2_.print(clamped);
    u8g2_.print("%");
  }

  u8g2_.sendBuffer();
}

void Display::showThreeLinesWithProgress(const char* line1, const char* line2, const char* line3,
                                         int progressPercent) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_6x10_tf);

  const char* lines[] = {line1, line2, line3};
  const int baseline[] = {10, 22, 34};
  for (int i = 0; i < 3; ++i) {
    if (lines[i] == nullptr || lines[i][0] == '\0') {
      continue;
    }
    const int w = u8g2_.getStrWidth(lines[i]);
    u8g2_.setCursor((128 - w) / 2, baseline[i]);
    u8g2_.print(lines[i]);
  }

  const int clamped = constrain(progressPercent, 0, 100);
  u8g2_.drawFrame(0, 48, 128, 14);
  const int bar = (124 * clamped) / 100;
  u8g2_.drawBox(2, 50, bar, 10);
  u8g2_.setCursor(94, 62);
  u8g2_.print(clamped);
  u8g2_.print("%");

  u8g2_.sendBuffer();
}

void Display::showThreeLinesCentered(const char* line1, const char* line2, const char* line3) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_unifont_t_polish);

  const char* lines[] = {line1, line2, line3};
  const int baseline[] = {16, 34, 52};
  for (int i = 0; i < 3; ++i) {
    if (lines[i] == nullptr || lines[i][0] == '\0') {
      continue;
    }
    const int w = u8g2_.getUTF8Width(lines[i]);
    u8g2_.setCursor((128 - w) / 2, baseline[i]);
    u8g2_.print(lines[i]);
  }
  u8g2_.sendBuffer();
}

void Display::showThreeLinesLeft(const char* line1, const char* line2, const char* line3,
                                 int progressPercent) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_6x10_tf);

  const char* lines[] = {line1, line2, line3};
  const bool withProgress = progressPercent >= 0;
  const int baselineNoBar[] = {10, 24, 38};
  const int baselineBar[] = {10, 22, 34};
  const int* baselines = withProgress ? baselineBar : baselineNoBar;
  for (int i = 0; i < 3; ++i) {
    if (lines[i] == nullptr || lines[i][0] == '\0') {
      continue;
    }
    u8g2_.setCursor(0, baselines[i]);
    u8g2_.print(lines[i]);
  }

  if (withProgress) {
    const int clamped = constrain(progressPercent, 0, 100);
    u8g2_.drawFrame(0, 48, 128, 14);
    const int bar = (124 * clamped) / 100;
    u8g2_.drawBox(2, 50, bar, 10);
    u8g2_.setCursor(94, 62);
    u8g2_.print(clamped);
    u8g2_.print("%");
  }

  u8g2_.sendBuffer();
}

void Display::showWelcome(const WelcomeMessage& message) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_unifont_t_polish);

  const char* lines[] = {message.line1.c_str(), message.line2.c_str(), message.line3.c_str()};
  const int baseline[] = {14, 30, 46};
  for (int i = 0; i < 3; ++i) {
    if (lines[i][0] == '\0') {
      continue;
    }
    const int w = u8g2_.getUTF8Width(lines[i]);
    u8g2_.setCursor((128 - w) / 2, baseline[i]);
    u8g2_.print(lines[i]);
  }
  u8g2_.sendBuffer();
}

void Display::showServiceDateTime(const char* header, const char* dateLine, const char* timeLine) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.setCursor(0, 10);
  u8g2_.print(header);

  if (dateLine != nullptr && dateLine[0] != '\0') {
    u8g2_.setFont(u8g2_font_courB10_tf);
    int w = u8g2_.getStrWidth(dateLine);
    u8g2_.setCursor((128 - w) / 2, 28);
    u8g2_.print(dateLine);
  }

  if (timeLine != nullptr && timeLine[0] != '\0') {
    u8g2_.setFont(u8g2_font_courB14_tf);
    int w = u8g2_.getStrWidth(timeLine);
    u8g2_.setCursor((128 - w) / 2, 48);
    u8g2_.print(timeLine);
  }

  u8g2_.sendBuffer();
}

void Display::showTimeMenuRoot(const char* dateLine, const char* timeLine) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.setCursor(0, 8);
  u8g2_.print("Czas / DS3231");

  if (dateLine != nullptr && dateLine[0] != '\0') {
    u8g2_.setFont(u8g2_font_courB10_tf);
    int w = u8g2_.getStrWidth(dateLine);
    u8g2_.setCursor((128 - w) / 2, 24);
    u8g2_.print(dateLine);
  }

  if (timeLine != nullptr && timeLine[0] != '\0') {
    u8g2_.setFont(u8g2_font_courB14_tf);
    int w = u8g2_.getStrWidth(timeLine);
    u8g2_.setCursor((128 - w) / 2, 42);
    u8g2_.print(timeLine);
  }

  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.setCursor(0, 54);
  u8g2_.print("1:Get 2:Recz Tara:wyj");
  u8g2_.sendBuffer();
}

void Display::showCalWeightEdit(int grams) {
  char valueBuf[16];
  snprintf(valueBuf, sizeof(valueBuf), "%d g", grams);

  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_courB10_tf);
  u8g2_.setCursor(0, 14);
  u8g2_.print("Kalibracja 2/2");
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.setCursor(0, 26);
  u8g2_.print("Masa wzorcowa:");
  u8g2_.setFont(u8g2_font_courB18_tf);
  const int w = u8g2_.getStrWidth(valueBuf);
  u8g2_.setCursor((128 - w) / 2, 50);
  u8g2_.print(valueBuf);
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.setCursor(0, 62);
  u8g2_.print("1/+ 2/- OK=kalibruj");
  u8g2_.sendBuffer();
}

void Display::showInfoScreen(const char* const lines[], uint8_t count) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_6x10_tf);
  for (uint8_t i = 0; i < count && i < 6; ++i) {
    if (lines[i] == nullptr) {
      continue;
    }
    u8g2_.setCursor(0, 9 + i * 11);
    u8g2_.print(lines[i]);
  }
  u8g2_.sendBuffer();
}

void Display::showTimeManualEdit(const char* fieldLabel, int value) {
  char valueBuf[16];
  snprintf(valueBuf, sizeof(valueBuf), "%d", value);

  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_courB10_tf);
  u8g2_.setCursor(0, 14);
  u8g2_.print("Recznie:");
  u8g2_.setCursor(0, 28);
  u8g2_.print(fieldLabel);
  u8g2_.setFont(u8g2_font_courB18_tf);
  const int w = u8g2_.getStrWidth(valueBuf);
  u8g2_.setCursor((128 - w) / 2, 50);
  u8g2_.print(valueBuf);
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.setCursor(0, 62);
  u8g2_.print("1/+ 2/- OK=dalej Tara");
  u8g2_.sendBuffer();
}

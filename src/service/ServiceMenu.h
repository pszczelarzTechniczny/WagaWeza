#ifndef SERVICE_MENU_H
#define SERVICE_MENU_H

#include <Arduino.h>

#include "../input/InputButtons.h"
#include "../ui/Display.h"

enum class ServiceMenuAction : uint8_t {
  NONE,
  TEST_WS,
  INFO,
  TIME,
  UPDATE,
  CALIBRATION,
  TARA_SAVE,
  EXIT,
};

// Nawigowane menu trybu serwisowego: nagłówek w negatywie (SERWIS + IP),
// lista pozycji z ikonami i kursorem. 1 = góra, 2 = dół, OK = wybór,
// Tara = wyjście z trybu serwisowego.
class ServiceMenu {
 public:
  explicit ServiceMenu(Display& display);

  void enter(const char* ipText);
  // Wymusza przerysowanie (po powrocie z pod-ekranu/komunikatu).
  void requestRedraw();
  // Obsługa przycisków + rysowanie. Zwraca wybraną akcję (NONE gdy brak).
  ServiceMenuAction tick(InputButtons& buttons);

 private:
  static const uint8_t kVisibleRows = 5;

  void draw();

  Display& display_;
  String ip_;
  uint8_t cursor_;
  uint8_t scrollTop_;
  bool dirty_;
};

#endif

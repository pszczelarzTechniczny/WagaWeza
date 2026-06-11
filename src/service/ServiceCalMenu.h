#ifndef SERVICE_CAL_MENU_H
#define SERVICE_CAL_MENU_H

#include <Arduino.h>

#include "ScaleServiceActions.h"
#include "../input/InputButtons.h"
#include "../ui/Display.h"

class ServiceCalMenu {
 public:
  ServiceCalMenu(ScaleServiceActions& actions, Display& display);

  bool isActive() const;
  void enter();
  void exit();

  // Zwraca false, gdy menu zostało zamknięte w tym ticku.
  bool tick(InputButtons& buttons);

 private:
  enum class State : uint8_t {
    INACTIVE,
    EMPTY_PROMPT,
    WEIGHT_EDIT,
    RESULT,
  };

  void showEmptyPrompt();
  void showWeightEdit();

  ScaleServiceActions& actions_;
  Display& display_;
  State state_;
  unsigned long stateEnteredMs_;
  int grams_;
  unsigned long holdStartMs_;
  unsigned long lastRepeatMs_;
};

#endif

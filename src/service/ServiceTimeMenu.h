#ifndef SERVICE_TIME_MENU_H
#define SERVICE_TIME_MENU_H

#include <Arduino.h>

#include "../input/InputButtons.h"
#include "../ui/Display.h"
#include "ClockServiceActions.h"

class ServiceTimeMenu {
 public:
  ServiceTimeMenu(ClockServiceActions& clockActions, Display& display);

  bool isActive() const;
  void enter();
  void exit();

  bool tick(InputButtons& buttons);

 private:
  enum class State : uint8_t {
    INACTIVE,
    ROOT,
    NTP_WORKING,
    NTP_RESULT,
    MANUAL,
    MANUAL_SAVED,
  };

  enum class ManualField : uint8_t { HOUR, MINUTE, DAY, MONTH, YEAR, COUNT };

  void showRoot();
  void showManual();
  void startNtp();
  void adjustManual(int delta);
  void nextManualField();
  bool saveManual();
  int manualFieldValue() const;
  void setManualFieldValue(int value);
  int manualFieldMax() const;

  ClockServiceActions& clockActions_;
  Display& display_;

  State state_;
  unsigned long stateEnteredMs_;
  unsigned long lastUiMs_;

  String ntpResultMessage_;

  RtcDateTime edit_;
  ManualField manualField_;
};

#endif

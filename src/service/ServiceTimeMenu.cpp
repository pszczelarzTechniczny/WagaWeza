#include "ServiceTimeMenu.h"

#include "../../config/Buttons.h"
#include "../rtc/RtcClock.h"

ServiceTimeMenu::ServiceTimeMenu(ClockServiceActions& clockActions, Display& display)
    : clockActions_(clockActions),
      display_(display),
      state_(State::INACTIVE),
      stateEnteredMs_(0),
      lastUiMs_(0),
      manualField_(ManualField::HOUR) {}

bool ServiceTimeMenu::isActive() const { return state_ != State::INACTIVE; }

void ServiceTimeMenu::enter() {
  state_ = State::ROOT;
  stateEnteredMs_ = millis();
  lastUiMs_ = 0;
  showRoot();
}

void ServiceTimeMenu::exit() {
  state_ = State::INACTIVE;
}

void ServiceTimeMenu::showRoot() {
  String dateLine;
  String timeLine;
  clockActions_.currentTimeLines(dateLine, timeLine);
  display_.showTimeMenuRoot(dateLine.c_str(), timeLine.c_str());
}

void ServiceTimeMenu::showManual() {
  const char* label = "?";
  switch (manualField_) {
    case ManualField::HOUR:
      label = "Godzina";
      break;
    case ManualField::MINUTE:
      label = "Minuta";
      break;
    case ManualField::DAY:
      label = "Dzien";
      break;
    case ManualField::MONTH:
      label = "Miesiac";
      break;
    case ManualField::YEAR:
      label = "Rok";
      break;
    default:
      break;
  }
  display_.showTimeManualEdit(label, manualFieldValue());
}

void ServiceTimeMenu::startNtp() {
  state_ = State::NTP_WORKING;
  stateEnteredMs_ = millis();
  display_.showTwoLines("Get time", "Laczenie NTP...");
  ntpResultMessage_ = clockActions_.syncFromInternet();
  state_ = State::NTP_RESULT;
  stateEnteredMs_ = millis();
  display_.showTwoLines("Get time", ntpResultMessage_.c_str());
}

void ServiceTimeMenu::adjustManual(int delta) {
  int value = manualFieldValue() + delta;
  const int maxVal = manualFieldMax();
  int minVal = 0;

  switch (manualField_) {
    case ManualField::HOUR:
      minVal = 0;
      break;
    case ManualField::MINUTE:
      minVal = 0;
      break;
    case ManualField::DAY:
      minVal = 1;
      break;
    case ManualField::MONTH:
      minVal = 1;
      break;
    case ManualField::YEAR:
      minVal = 2020;
      break;
    default:
      break;
  }

  if (value > maxVal) {
    value = minVal;
  }
  if (value < minVal) {
    value = maxVal;
  }

  setManualFieldValue(value);
  showManual();
}

void ServiceTimeMenu::nextManualField() {
  const int next = static_cast<int>(manualField_) + 1;
  if (next >= static_cast<int>(ManualField::COUNT)) {
    if (saveManual()) {
      state_ = State::MANUAL_SAVED;
      stateEnteredMs_ = millis();
      display_.showTwoLines("Zapisano", RtcClock::format(edit_).c_str());
    } else {
      display_.showTwoLines("Blad", "Zapis DS3231");
    }
    return;
  }

  manualField_ = static_cast<ManualField>(next);
  showManual();
}

bool ServiceTimeMenu::saveManual() {
  edit_.second = 0;
  if (!RtcClock::isValid(edit_)) {
    return false;
  }
  return clockActions_.writeRtc(edit_);
}

int ServiceTimeMenu::manualFieldValue() const {
  switch (manualField_) {
    case ManualField::HOUR:
      return edit_.hour;
    case ManualField::MINUTE:
      return edit_.minute;
    case ManualField::DAY:
      return edit_.day;
    case ManualField::MONTH:
      return edit_.month;
    case ManualField::YEAR:
      return edit_.year;
    default:
      return 0;
  }
}

void ServiceTimeMenu::setManualFieldValue(int value) {
  switch (manualField_) {
    case ManualField::HOUR:
      edit_.hour = static_cast<uint8_t>(value);
      break;
    case ManualField::MINUTE:
      edit_.minute = static_cast<uint8_t>(value);
      break;
    case ManualField::DAY:
      edit_.day = static_cast<uint8_t>(value);
      break;
    case ManualField::MONTH:
      edit_.month = static_cast<uint8_t>(value);
      break;
    case ManualField::YEAR:
      edit_.year = static_cast<uint16_t>(value);
      break;
    default:
      break;
  }
}

int ServiceTimeMenu::manualFieldMax() const {
  switch (manualField_) {
    case ManualField::HOUR:
      return 23;
    case ManualField::MINUTE:
      return 59;
    case ManualField::DAY:
      return 31;
    case ManualField::MONTH:
      return 12;
    case ManualField::YEAR:
      return 2099;
    default:
      return 0;
  }
}

bool ServiceTimeMenu::tick(InputButtons& buttons) {
  if (!isActive()) {
    return false;
  }

  const unsigned long now = millis();

  if (state_ == State::NTP_RESULT || state_ == State::MANUAL_SAVED) {
    if (now - stateEnteredMs_ > 2500) {
      state_ = State::ROOT;
      showRoot();
    }
    if (buttons.wasPressed(BTN_TARA) || buttons.wasPressed(BTN_OK)) {
      state_ = State::ROOT;
      showRoot();
    }
    return true;
  }

  if (state_ == State::NTP_WORKING) {
    return true;
  }

  if (state_ == State::ROOT) {
    if (now - lastUiMs_ > 3000) {
      lastUiMs_ = now;
      showRoot();
    }

    if (buttons.wasPressed(BTN_1)) {
      startNtp();
      return true;
    }
    if (buttons.wasPressed(BTN_2)) {
      if (!clockActions_.readRtc(edit_)) {
        edit_ = {};
        edit_.year = 2026;
        edit_.month = 1;
        edit_.day = 1;
      }
      edit_.second = 0;
      manualField_ = ManualField::HOUR;
      state_ = State::MANUAL;
      showManual();
      return true;
    }
    if (buttons.wasPressed(BTN_TARA)) {
      exit();
      return false;
    }
    return true;
  }

  if (state_ == State::MANUAL) {
    if (buttons.wasPressed(BTN_1)) {
      adjustManual(1);
    } else if (buttons.wasPressed(BTN_2)) {
      adjustManual(-1);
    } else if (buttons.wasPressed(BTN_OK)) {
      nextManualField();
    } else if (buttons.wasPressed(BTN_TARA)) {
      state_ = State::ROOT;
      showRoot();
    }
    return true;
  }

  return true;
}

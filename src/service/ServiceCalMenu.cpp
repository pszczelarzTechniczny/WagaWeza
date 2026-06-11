#include "ServiceCalMenu.h"

#include "../../config/Buttons.h"

ServiceCalMenu::ServiceCalMenu(ScaleServiceActions& actions, Display& display)
    : actions_(actions),
      display_(display),
      state_(State::INACTIVE),
      stateEnteredMs_(0),
      grams_(5000),
      holdStartMs_(0),
      lastRepeatMs_(0) {}

bool ServiceCalMenu::isActive() const { return state_ != State::INACTIVE; }

void ServiceCalMenu::enter() {
  state_ = State::EMPTY_PROMPT;
  stateEnteredMs_ = millis();
  showEmptyPrompt();
}

void ServiceCalMenu::exit() { state_ = State::INACTIVE; }

void ServiceCalMenu::showEmptyPrompt() {
  display_.showThreeLinesLeft("Kalibracja 1/2", "Oprozni wage", "OK=dalej Tara=anuluj");
}

void ServiceCalMenu::showWeightEdit() { display_.showCalWeightEdit(grams_); }

bool ServiceCalMenu::tick(InputButtons& buttons) {
  if (!isActive()) {
    return false;
  }

  const unsigned long now = millis();

  if (state_ == State::RESULT) {
    if (now - stateEnteredMs_ > 2500 || buttons.wasPressed(BTN_OK) ||
        buttons.wasPressed(BTN_TARA)) {
      exit();
      return false;
    }
    return true;
  }

  if (state_ == State::EMPTY_PROMPT) {
    if (buttons.wasPressed(BTN_OK)) {
      bool ok = false;
      const String msg = actions_.calibrateEmpty(&ok);
      if (!ok) {
        display_.showTwoLines("Blad", msg.c_str());
        state_ = State::RESULT;
        stateEnteredMs_ = millis();
        return true;
      }
      grams_ = actions_.defaultCalWeightGrams();
      if (grams_ < 100) {
        grams_ = 5000;
      }
      holdStartMs_ = 0;
      state_ = State::WEIGHT_EDIT;
      showWeightEdit();
      return true;
    }
    if (buttons.wasPressed(BTN_TARA)) {
      exit();
      return false;
    }
    return true;
  }

  if (state_ == State::WEIGHT_EDIT) {
    int dir = 0;
    if (buttons.wasPressed(BTN_1)) {
      dir = 1;
    } else if (buttons.wasPressed(BTN_2)) {
      dir = -1;
    }

    if (dir != 0) {
      holdStartMs_ = now;
      lastRepeatMs_ = now;
      grams_ = constrain(grams_ + dir * 50, 100, Scale::kMaxWeightGrams);
      showWeightEdit();
    } else if (buttons.isHeld(BTN_1) || buttons.isHeld(BTN_2)) {
      // Autorepeat przy przytrzymaniu, po 3 s krok rośnie do 500 g.
      if (holdStartMs_ != 0 && now - holdStartMs_ > 600 && now - lastRepeatMs_ > 120) {
        lastRepeatMs_ = now;
        const int step = (now - holdStartMs_ > 3000) ? 500 : 50;
        const int delta = buttons.isHeld(BTN_1) ? step : -step;
        grams_ = constrain(grams_ + delta, 100, Scale::kMaxWeightGrams);
        showWeightEdit();
      }
    } else {
      holdStartMs_ = 0;
    }

    if (buttons.wasPressed(BTN_OK)) {
      display_.showTwoLines("Kalibracja", "Pomiar...");
      bool ok = false;
      const String msg = actions_.calibrateWithWeight(grams_, &ok);
      display_.showTwoLines(ok ? "Kalibracja" : "Blad", msg.c_str());
      state_ = State::RESULT;
      stateEnteredMs_ = millis();
      return true;
    }
    if (buttons.wasPressed(BTN_TARA)) {
      exit();
      return false;
    }
    return true;
  }

  return true;
}

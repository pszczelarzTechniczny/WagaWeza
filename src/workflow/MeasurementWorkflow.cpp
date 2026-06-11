#include "MeasurementWorkflow.h"

MeasurementWorkflow::MeasurementWorkflow()
    : link_(nullptr),
      display_(nullptr),
      buzzer_(nullptr),
      state_(State::IDLE),
      sentAtMs_(0),
      resultUntilMs_(0),
      retried_(false),
      slot_(0),
      kg_(0.0f) {}

void MeasurementWorkflow::begin(PosLink* link, Display* display, BuzzerCallback buzzer) {
  link_ = link;
  display_ = display;
  buzzer_ = buzzer;
}

bool MeasurementWorkflow::isActive() const { return state_ != State::IDLE; }

void MeasurementWorkflow::showResult(const char* line1, const char* line2, bool success) {
  display_->showTwoLines(line1, line2);
  if (buzzer_ != nullptr) {
    buzzer_(success);
  }
  state_ = State::SHOW_RESULT;
  resultUntilMs_ = millis() + kResultMs;
}

const char* MeasurementWorkflow::ackErrorText(const String& code) {
  if (code == "slot-not-mapped") return "Slot bez produktu";
  if (code == "product-inactive") return "Produkt nieaktywny";
  if (code == "product-not-weighed") return "Nie na wage";
  if (code == "bad-weight") return "Najpierw poloz towar";
  return "Blad POS";
}

bool MeasurementWorkflow::tick(InputButtons& buttons, Scale& scale,
                               const StabilityTracker& stability) {
  const unsigned long now = millis();

  if (state_ == State::SHOW_RESULT) {
    if (now >= resultUntilMs_) {
      state_ = State::IDLE;
      return false;
    }
    return true;
  }

  if (state_ == State::WAIT_ACK) {
    PosAck ack;
    if (link_->takeAck(ack) && ack.eventId != eventId_) {
      Serial.printf("[pomiar] ack zignorowany (stary eventId %s)\n", ack.eventId.c_str());
    } else if (ack.received) {
      if (ack.ok) {
        char line2[24];
        snprintf(line2, sizeof(line2), "%.3f kg", kg_);
        char line1[16];
        snprintf(line1, sizeof(line1), "OK slot %u", (unsigned)slot_);
        Serial.printf("[pomiar] ack ok slot %u %.3f kg\n", (unsigned)slot_, kg_);
        showResult(line1, line2, true);
      } else {
        Serial.printf("[pomiar] ack blad: %s\n", ack.error.c_str());
        showResult("Blad", ackErrorText(ack.error), false);
      }
      return true;
    }

    if (now - sentAtMs_ > kAckTimeoutMs) {
      if (!retried_) {
        retried_ = true;
        sentAtMs_ = now;
        Serial.println("[pomiar] brak ack — retry");
        link_->resendButton(eventId_, slot_, kg_);
      } else {
        showResult("Brak polaczenia", "z POS", false);
      }
    }
    return true;
  }

  // IDLE — skan przyciskow slotow.
  for (uint8_t i = BTN_1; i <= BTN_6; ++i) {
    if (!buttons.wasPressed(i)) {
      continue;
    }
    const uint8_t slotNumber = actionButtonPrefIndex(i) + 1;  // 1–6

    if (!link_->isConnected()) {
      showResult("Brak serwera", "POS niedostepny", false);
      return true;
    }

    const int grams = scale.readNetGrams(scale.runtimeTara());
    if (grams > Scale::kMaxWeightGrams) {
      showResult("Przeciazenie", "Zdejmij towar", false);
      return true;
    }
    if (grams <= 0 || !stability.isStable(now)) {
      showResult("Poloz towar", "i poczekaj", false);
      return true;
    }

    slot_ = slotNumber;
    kg_ = grams / 1000.0f;
    eventId_ = link_->sendButton(slot_, kg_);
    if (eventId_.length() == 0) {
      showResult("Brak serwera", "POS niedostepny", false);
      return true;
    }
    Serial.printf("[pomiar] slot %u %.3f kg eventId=%s\n", (unsigned)slot_, kg_,
                  eventId_.c_str());
    sentAtMs_ = now;
    retried_ = false;
    char line[20];
    snprintf(line, sizeof(line), "Slot %u...", (unsigned)slot_);
    display_->showTwoLines("Wysylanie", line);
    state_ = State::WAIT_ACK;
    return true;
  }

  return false;
}

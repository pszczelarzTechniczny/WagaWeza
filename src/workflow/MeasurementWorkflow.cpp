#include "MeasurementWorkflow.h"

MeasurementWorkflow::MeasurementWorkflow() : sender_(nullptr), display_(nullptr) {}

void MeasurementWorkflow::begin(MeasurementSender* sender, Display* display) {
  sender_ = sender;
  display_ = display;
}

bool MeasurementWorkflow::tick(InputButtons& buttons, Scale& scale) {
  if (sender_ != nullptr && sender_->isActive()) {
    sender_->tick();
    return true;
  }

  for (uint8_t i = BTN_1; i <= BTN_6; ++i) {
    if (buttons.wasPressed(i)) {
      const int weight = scale.readNetGrams(scale.runtimeTara());
      sender_->startSend(actionButtonPrefIndex(i), weight);
      return true;
    }
  }

  return false;
}

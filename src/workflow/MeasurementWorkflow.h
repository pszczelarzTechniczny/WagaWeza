#ifndef MEASUREMENT_WORKFLOW_H
#define MEASUREMENT_WORKFLOW_H

#include <Arduino.h>

#include "../../config/Buttons.h"
#include "../input/InputButtons.h"
#include "../network/MeasurementSender.h"
#include "../scale/Scale.h"
#include "../ui/Display.h"

class MeasurementWorkflow {
 public:
  MeasurementWorkflow();

  void begin(MeasurementSender* sender, Display* display);
  bool tick(InputButtons& buttons, Scale& scale);

 private:
  MeasurementSender* sender_;
  Display* display_;
};

#endif

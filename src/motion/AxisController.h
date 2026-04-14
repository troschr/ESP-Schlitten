#pragma once

#include <Arduino.h>

#include "config/Config.h"
#include "config/Pins.h"
#include "core/Types.h"

namespace esp_schlitten {

class AxisController {
 public:
  void begin();
  void moveTo(int32_t targetSteps, uint32_t speedStepsPerSecond);
  void update(uint32_t nowMicros);
  void stop();
  void resetPosition(int32_t positionSteps);

  int32_t currentPosition() const;
  int32_t targetPosition() const;
  Direction direction() const;
  bool isMoving() const;
  bool driverFault() const;

 private:
  void setEnabled(bool enabled);
  void pulseStepPin();

  int32_t currentPositionSteps_ = 0;
  int32_t targetPositionSteps_ = 0;
  Direction direction_ = Direction::None;
  uint32_t lastStepAtMicros_ = 0;
  uint32_t stepIntervalMicros_ = 0;
  bool moving_ = false;
};

}  // namespace esp_schlitten

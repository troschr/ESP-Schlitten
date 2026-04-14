#include "motion/AxisController.h"

namespace esp_schlitten {

void AxisController::begin() {
  if (pins::kAxisStepPin >= 0) {
    pinMode(pins::kAxisStepPin, OUTPUT);
    digitalWrite(pins::kAxisStepPin, LOW);
  }

  if (pins::kAxisDirPin >= 0) {
    pinMode(pins::kAxisDirPin, OUTPUT);
    digitalWrite(pins::kAxisDirPin, LOW);
  }

  if (pins::kAxisEnablePin >= 0) {
    pinMode(pins::kAxisEnablePin, OUTPUT);
  }

  setEnabled(false);
}

void AxisController::moveTo(int32_t targetSteps, uint32_t speedStepsPerSecond) {
  targetPositionSteps_ = targetSteps;

  if (targetPositionSteps_ == currentPositionSteps_) {
    direction_ = Direction::None;
    moving_ = false;
    setEnabled(false);
    return;
  }

  direction_ = targetPositionSteps_ > currentPositionSteps_ ? Direction::Positive
                                                            : Direction::Negative;
  const uint32_t speed = speedStepsPerSecond == 0
                             ? config::Motion::kDefaultMoveSpeedStepsPerSecond
                             : speedStepsPerSecond;
  stepIntervalMicros_ = 1000000UL / speed;
  lastStepAtMicros_ = 0;
  moving_ = true;

  if (pins::kAxisDirPin >= 0) {
    digitalWrite(pins::kAxisDirPin, direction_ == Direction::Positive ? HIGH : LOW);
  }
  setEnabled(true);
}

void AxisController::update(uint32_t nowMicros) {
  if (!moving_) {
    return;
  }

  if (lastStepAtMicros_ != 0 && (nowMicros - lastStepAtMicros_) < stepIntervalMicros_) {
    return;
  }

  if (currentPositionSteps_ == targetPositionSteps_) {
    stop();
    return;
  }

  pulseStepPin();
  currentPositionSteps_ += direction_ == Direction::Positive ? 1 : -1;
  lastStepAtMicros_ = nowMicros;

  if (currentPositionSteps_ == targetPositionSteps_) {
    stop();
  }
}

void AxisController::stop() {
  moving_ = false;
  direction_ = Direction::None;
  setEnabled(false);
}

void AxisController::resetPosition(int32_t positionSteps) {
  currentPositionSteps_ = positionSteps;
  targetPositionSteps_ = positionSteps;
}

int32_t AxisController::currentPosition() const {
  return currentPositionSteps_;
}

int32_t AxisController::targetPosition() const {
  return targetPositionSteps_;
}

Direction AxisController::direction() const {
  return direction_;
}

bool AxisController::isMoving() const {
  return moving_;
}

bool AxisController::driverFault() const {
  return false;
}

void AxisController::setEnabled(bool enabled) {
  if (pins::kAxisEnablePin < 0) {
    return;
  }

  const bool activeLevel = config::Motion::kEnableActiveLow ? LOW : HIGH;
  const bool inactiveLevel = config::Motion::kEnableActiveLow ? HIGH : LOW;
  digitalWrite(pins::kAxisEnablePin, enabled ? activeLevel : inactiveLevel);
}

void AxisController::pulseStepPin() {
  if (pins::kAxisStepPin < 0) {
    return;
  }

  digitalWrite(pins::kAxisStepPin, HIGH);
  digitalWrite(pins::kAxisStepPin, LOW);
}

}  // namespace esp_schlitten

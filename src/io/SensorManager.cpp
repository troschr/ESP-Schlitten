#include "io/SensorManager.h"

namespace esp_schlitten {

void SensorManager::begin() {
  if (pins::kGripperDetectPin >= 0) {
    pinMode(pins::kGripperDetectPin, INPUT_PULLUP);
  }

  if (pins::kHomeDetectPin >= 0) {
    pinMode(pins::kHomeDetectPin, INPUT_PULLUP);
  }

  Wire.begin();

  snapshot_.frontDistanceMm = config::Sensor::kDefaultClearDistanceMm;
  snapshot_.rearDistanceMm = config::Sensor::kDefaultClearDistanceMm;
  snapshot_.tofHealthy = true;
}

void SensorManager::update(uint32_t nowMs) {
  if ((nowMs - lastPollAtMs_) < config::Sensor::kPollIntervalMs) {
    return;
  }

  lastPollAtMs_ = nowMs;
  snapshot_.updatedAtMs = nowMs;
  snapshot_.gripperDetected =
      readDigitalSensor(pins::kGripperDetectPin, config::Sensor::kGripperActiveLow);
  snapshot_.homeDetected =
      readDigitalSensor(pins::kHomeDetectPin, config::Sensor::kHomeActiveLow);

  snapshot_.frontDistanceMm = config::Sensor::kDefaultClearDistanceMm;
  snapshot_.rearDistanceMm = config::Sensor::kDefaultClearDistanceMm;
  snapshot_.frontObstacle =
      snapshot_.frontDistanceMm <= config::Sensor::kStopDistanceMm;
  snapshot_.rearObstacle =
      snapshot_.rearDistanceMm <= config::Sensor::kStopDistanceMm;
  snapshot_.tofHealthy = true;
}

SensorSnapshot SensorManager::snapshot() const {
  return snapshot_;
}

bool SensorManager::obstacleInDirection(Direction direction) const {
  switch (direction) {
    case Direction::Negative:
      return snapshot_.rearObstacle;
    case Direction::Positive:
      return snapshot_.frontObstacle;
    case Direction::None:
      return false;
  }

  return false;
}

bool SensorManager::readDigitalSensor(int8_t pin, bool activeLow) const {
  if (pin < 0) {
    return false;
  }

  const int rawValue = digitalRead(pin);
  return activeLow ? (rawValue == LOW) : (rawValue == HIGH);
}

}  // namespace esp_schlitten

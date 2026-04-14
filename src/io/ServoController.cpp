#include "io/ServoController.h"

namespace esp_schlitten {

void ServoController::begin() {
  if (pins::kServoPwmPin >= 0) {
    ledcSetup(config::Servo::kPwmChannel, config::Servo::kFrequencyHz,
              config::Servo::kResolutionBits);
    ledcAttachPin(pins::kServoPwmPin, config::Servo::kPwmChannel);
    attached_ = true;
  }

  setPosition(HolderPosition::Service);
}

bool ServoController::setPosition(HolderPosition position) {
  if (position == HolderPosition::Unknown) {
    return false;
  }

  position_ = position;
  if (!attached_) {
    return true;
  }

  ledcWrite(config::Servo::kPwmChannel, microsecondsToDuty(pulseWidthForPosition(position)));
  return true;
}

HolderPosition ServoController::position() const {
  return position_;
}

uint32_t ServoController::microsecondsToDuty(uint16_t microseconds) const {
  const uint32_t maxDuty = (1UL << config::Servo::kResolutionBits) - 1UL;
  return (static_cast<uint32_t>(microseconds) * maxDuty) / 20000UL;
}

uint16_t ServoController::pulseWidthForPosition(HolderPosition position) const {
  switch (position) {
    case HolderPosition::Open:
      return config::Servo::kOpenUs;
    case HolderPosition::Closed:
      return config::Servo::kClosedUs;
    case HolderPosition::Service:
      return config::Servo::kServiceUs;
    case HolderPosition::Unknown:
      return config::Servo::kServiceUs;
  }

  return config::Servo::kServiceUs;
}

}  // namespace esp_schlitten

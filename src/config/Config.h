#pragma once

#include <Arduino.h>

namespace esp_schlitten {
namespace config {

struct Serial {
  static constexpr uint32_t kBaudRate = 115200;
};

struct Sensor {
  static constexpr bool kGripperActiveLow = true;
  static constexpr bool kHomeActiveLow = true;
  static constexpr uint32_t kPollIntervalMs = 50;
  static constexpr uint16_t kDefaultClearDistanceMm = 1000;
  static constexpr uint16_t kWarnDistanceMm = 120;
  static constexpr uint16_t kStopDistanceMm = 60;
};

struct Motion {
  static constexpr bool kEnableActiveLow = true;
  static constexpr int32_t kHomePositionSteps = 0;
  static constexpr int32_t kHomingSearchDistanceSteps = 12000;
  static constexpr int32_t kMoveToleranceSteps = 8;
  static constexpr uint32_t kDefaultMoveSpeedStepsPerSecond = 1200;
  static constexpr uint32_t kDefaultMoveAccelerationStepsPerSecond2 = 200;
  static constexpr uint32_t kHomingSpeedStepsPerSecond = 400;
  static constexpr uint32_t kMoveTimeoutMs = 20000;
  static constexpr uint32_t kHomingTimeoutMs = 15000;
};

struct Servo {
  static constexpr uint8_t kPwmChannel = 2;
  static constexpr uint16_t kFrequencyHz = 50;
  static constexpr uint8_t kResolutionBits = 16;
  static constexpr uint16_t kOpenUs = 2000;
  static constexpr uint16_t kClosedUs = 1000;
  static constexpr uint16_t kServiceUs = 1500;
};

struct Safety {
  static constexpr uint32_t kPiHeartbeatTimeoutMs = 4000;
};

}  // namespace config
}  // namespace esp_schlitten

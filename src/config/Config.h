#pragma once

#include <Arduino.h>

namespace esp_schlitten {
namespace config {

struct Serial {
  static constexpr uint32_t kBaudRate = 115200;
};

struct Timing {
  static constexpr uint32_t kHeartbeatIntervalMs = 1000;  // TODO: mit Pi abstimmen
  static constexpr uint32_t kStreamIntervalMs    = 1000;
};

struct Sensor {
  // Türsensor: Distanz über diesem Wert gilt als "Tür offen"
  static constexpr uint16_t kDoorOpenThresholdMm = 200;  // TODO: am echten Aufbau bestätigen

  // Hindernissensor: Grenzen für Fahrtfreigabe
  static constexpr uint16_t kObstacleStopDistanceMm = 60;   // TODO: am echten Aufbau bestätigen
  static constexpr uint16_t kObstacleWarnDistanceMm = 120;  // TODO: am echten Aufbau bestätigen
};

struct Motion {
  // Alle Werte in mm – Umrechnung in Steps macht der jeweilige Achscontroller intern
  // TODO: steps_per_mm je Achse festlegen wenn Mechanik bekannt
  static constexpr uint32_t kMoveTimeoutMs   = 20000;  // TODO: am echten Aufbau bestätigen
  static constexpr uint32_t kHomingTimeoutMs = 15000;  // TODO: am echten Aufbau bestätigen
  static constexpr uint8_t  kPositionToleranceMm = 1;  // TODO: am echten Aufbau bestätigen

  static constexpr bool kEnableActiveLow = true;
};

struct ClampServo {
  static constexpr uint8_t  kPwmChannel     = 0;
  static constexpr uint16_t kFrequencyHz    = 50;
  static constexpr uint8_t  kResolutionBits = 16;
  static constexpr uint16_t kOpenUs         = 2000;  // Platte freigegeben
  static constexpr uint16_t kClosedUs       = 1000;  // Platte geklemmt
  static constexpr uint16_t kServiceUs      = 1500;  // Mittelstellung
};

}  // namespace config
}  // namespace esp_schlitten

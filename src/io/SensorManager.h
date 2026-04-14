#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "config/Config.h"
#include "config/Pins.h"
#include "core/Types.h"

namespace esp_schlitten {

class SensorManager {
 public:
  void begin();
  void update(uint32_t nowMs);
  SensorSnapshot snapshot() const;
  bool obstacleInDirection(Direction direction) const;

 private:
  bool readDigitalSensor(int8_t pin, bool activeLow) const;

  SensorSnapshot snapshot_;
  uint32_t lastPollAtMs_ = 0;
};

}  // namespace esp_schlitten

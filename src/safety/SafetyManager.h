#pragma once

#include <Arduino.h>

#include "config/Config.h"
#include "core/Types.h"

namespace esp_schlitten {

class SafetyManager {
 public:
  void begin(uint32_t nowMs);
  void notePiContact(uint32_t nowMs);
  ErrorCode update(uint32_t nowMs, const SensorSnapshot &sensors,
                   const MotionSnapshot &motion) const;

 private:
  uint32_t lastPiContactAtMs_ = 0;
  bool commWatchdogArmed_ = false;
};

}  // namespace esp_schlitten

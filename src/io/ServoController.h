#pragma once

#include <Arduino.h>

#include "config/Config.h"
#include "config/Pins.h"
#include "core/Types.h"

namespace esp_schlitten {

class ServoController {
 public:
  void begin();
  bool setPosition(HolderPosition position);
  HolderPosition position() const;

 private:
  uint32_t microsecondsToDuty(uint16_t microseconds) const;
  uint16_t pulseWidthForPosition(HolderPosition position) const;

  HolderPosition position_ = HolderPosition::Unknown;
  bool attached_ = false;
};

}  // namespace esp_schlitten

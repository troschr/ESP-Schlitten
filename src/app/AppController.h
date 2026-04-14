#pragma once

#include <Arduino.h>

#include "comm/CommandInterface.h"
#include "config/Config.h"
#include "core/Types.h"
#include "io/SensorManager.h"
#include "io/ServoController.h"
#include "motion/MotionController.h"
#include "safety/SafetyManager.h"
#include "status/StatusReporter.h"

namespace esp_schlitten {

class AppController {
 public:
  void begin();
  void update();

 private:
  void processCommands(uint32_t nowMs);
  void processCommand(const Command &command, uint32_t nowMs);
  void publishStatus();
  void setState(AppState nextState);
  void enterError(ErrorCode error);

  CommandInterface commandInterface_;
  SensorManager sensorManager_;
  ServoController servoController_;
  MotionController motionController_;
  SafetyManager safetyManager_;
  StatusReporter statusReporter_;

  AppState state_ = AppState::Booting;
  ErrorCode error_ = ErrorCode::None;
  uint32_t activeCommandId_ = 0;
};

}  // namespace esp_schlitten

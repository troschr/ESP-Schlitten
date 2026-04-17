#pragma once

#include <Arduino.h>

#include "comm/CommandInterface.h"
#include "config/Config.h"
#include "core/Types.h"
#include "status/StatusReporter.h"

namespace esp_schlitten {

class AppController {
 public:
  void begin();
  void update();

 private:
  void processCommands();
  void processCommand(const Command &cmd);
  void handleMoveTo(const Command &cmd);
  void handleHome(const Command &cmd);
  void handleSetClamp(const Command &cmd);
  void handleSetDoorArm(const Command &cmd);

  void setState(AppState next);
  void enterError(ErrorCode error);
  void publishStatus();
  MotionSnapshot motionSnapshot() const;
  SensorSnapshot sensorSnapshot() const;

  CommandInterface comm_;
  StatusReporter   reporter_;

  AppState  state_    = AppState::NotReferenced;
  ErrorCode error_    = ErrorCode::None;
  Position  current_;
  Position  target_;

  bool     streamEnabled_   = false;
  uint32_t lastStreamAtMs_  = 0;
  uint32_t lastHeartbeatAtMs_ = 0;
};

}  // namespace esp_schlitten

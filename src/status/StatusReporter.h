#pragma once

#include <Arduino.h>

#include "comm/CommandInterface.h"
#include "core/Types.h"

namespace esp_schlitten {

class StatusReporter {
 public:
  void begin(CommandInterface &commandInterface);
  void sendState(AppState state, const MotionSnapshot &motion);
  void sendStatus(const StatusSnapshot &snapshot);
  void sendError(ErrorCode error, const MotionSnapshot &motion);
  void sendOk(uint32_t id, const String &eventName, const MotionSnapshot &motion);

 private:
  CommandInterface *commandInterface_ = nullptr;
};

}  // namespace esp_schlitten

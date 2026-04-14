#pragma once

#include <Arduino.h>

#include "config/Config.h"
#include "core/Types.h"
#include "io/SensorManager.h"
#include "motion/AxisController.h"

namespace esp_schlitten {

class MotionController {
 public:
  struct UpdateResult {
    bool homeCompleted = false;
    bool moveCompleted = false;
    bool stopCompleted = false;
    ErrorCode fault = ErrorCode::None;
  };

  void begin();
  UpdateResult update(uint32_t nowMs, uint32_t nowMicros, const SensorSnapshot &sensors);

  bool startHoming(uint32_t nowMs);
  bool startMoveTo(int32_t targetSteps, uint32_t speedStepsPerSecond, uint32_t nowMs);
  void stopImmediate();
  void clearReference();

  MotionSnapshot snapshot() const;

  bool isBusy() const;
  bool isReferenced() const;
  bool isHoming() const;
  bool isMoving() const;

 private:
  enum class RuntimeState : uint8_t {
    Idle,
    Homing,
    Moving,
  };

  UpdateResult fail(ErrorCode error);

  AxisController axis_;
  RuntimeState state_ = RuntimeState::Idle;
  bool referenced_ = false;
  int32_t homingStartPositionSteps_ = 0;
  uint32_t operationStartedAtMs_ = 0;
};

}  // namespace esp_schlitten

#include "motion/MotionController.h"

namespace esp_schlitten {

void MotionController::begin() {
  axis_.begin();
  axis_.resetPosition(config::Motion::kHomePositionSteps);
  referenced_ = false;
  state_ = RuntimeState::Idle;
}

MotionController::UpdateResult MotionController::update(uint32_t nowMs, uint32_t nowMicros,
                                                        const SensorSnapshot &sensors) {
  UpdateResult result;
  axis_.update(nowMicros);

  if (axis_.driverFault()) {
    return fail(ErrorCode::DriverFault);
  }

  if (state_ == RuntimeState::Homing) {
    if (sensors.frontObstacle || sensors.rearObstacle) {
      return fail(ErrorCode::ObstacleDetected);
    }

    const bool searchDistanceReached =
        absoluteDistance(axis_.currentPosition(), homingStartPositionSteps_) >=
        config::Motion::kHomingSearchDistanceSteps;
    if (sensors.homeDetected || searchDistanceReached) {
      axis_.stop();
      axis_.resetPosition(config::Motion::kHomePositionSteps);
      referenced_ = true;
      state_ = RuntimeState::Idle;
      result.homeCompleted = true;
      return result;
    }

    if ((nowMs - operationStartedAtMs_) > config::Motion::kHomingTimeoutMs) {
      return fail(ErrorCode::HomingTimeout);
    }
  } else if (state_ == RuntimeState::Moving) {
    if ((axis_.direction() == Direction::Positive && sensors.frontObstacle) ||
        (axis_.direction() == Direction::Negative && sensors.rearObstacle)) {
      return fail(ErrorCode::ObstacleDetected);
    }

    if ((nowMs - operationStartedAtMs_) > config::Motion::kMoveTimeoutMs) {
      return fail(ErrorCode::MoveTimeout);
    }

    if (!axis_.isMoving()) {
      if (absoluteDistance(axis_.currentPosition(), axis_.targetPosition()) <=
          config::Motion::kMoveToleranceSteps) {
        state_ = RuntimeState::Idle;
        result.moveCompleted = true;
      } else {
        return fail(ErrorCode::PositionError);
      }
    }
  }

  return result;
}

bool MotionController::startHoming(uint32_t nowMs) {
  if (state_ != RuntimeState::Idle) {
    return false;
  }

  operationStartedAtMs_ = nowMs;
  homingStartPositionSteps_ = axis_.currentPosition();
  state_ = RuntimeState::Homing;
  referenced_ = false;
  axis_.moveTo(homingStartPositionSteps_ - config::Motion::kHomingSearchDistanceSteps,
               config::Motion::kHomingSpeedStepsPerSecond);
  return true;
}

bool MotionController::startMoveTo(int32_t targetSteps, uint32_t speedStepsPerSecond,
                                   uint32_t nowMs) {
  if (state_ != RuntimeState::Idle || !referenced_) {
    return false;
  }

  operationStartedAtMs_ = nowMs;
  state_ = RuntimeState::Moving;
  axis_.moveTo(targetSteps, speedStepsPerSecond);
  return true;
}

void MotionController::stopImmediate() {
  axis_.stop();
  state_ = RuntimeState::Idle;
}

void MotionController::clearReference() {
  referenced_ = false;
  axis_.resetPosition(config::Motion::kHomePositionSteps);
  state_ = RuntimeState::Idle;
}

MotionSnapshot MotionController::snapshot() const {
  MotionSnapshot snapshot;
  snapshot.currentPositionSteps = axis_.currentPosition();
  snapshot.targetPositionSteps = axis_.targetPosition();
  snapshot.busy = state_ != RuntimeState::Idle;
  snapshot.homing = state_ == RuntimeState::Homing;
  snapshot.moving = state_ == RuntimeState::Moving;
  snapshot.referenced = referenced_;
  snapshot.direction = axis_.direction();
  return snapshot;
}

bool MotionController::isBusy() const {
  return state_ != RuntimeState::Idle;
}

bool MotionController::isReferenced() const {
  return referenced_;
}

bool MotionController::isHoming() const {
  return state_ == RuntimeState::Homing;
}

bool MotionController::isMoving() const {
  return state_ == RuntimeState::Moving;
}

MotionController::UpdateResult MotionController::fail(ErrorCode error) {
  axis_.stop();
  state_ = RuntimeState::Idle;

  UpdateResult result;
  result.fault = error;
  return result;
}

}  // namespace esp_schlitten

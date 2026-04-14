#pragma once

#include <Arduino.h>

namespace esp_schlitten {

enum class AppState : uint8_t {
  Booting,
  NotReferenced,
  Ready,
  BusyHoming,
  BusyMoving,
  Stopped,
  Error,
};

enum class ErrorCode : uint8_t {
  None,
  InvalidCommand,
  Busy,
  NotReferenced,
  ObstacleDetected,
  MoveTimeout,
  HomingTimeout,
  PositionError,
  SensorFaultTof,
  SensorFaultGripper,
  DriverFault,
  CommTimeoutPi,
  InvalidState,
};

enum class CommandType : uint8_t {
  None,
  Status,
  Stop,
  Home,
  MoveTo,
  ResetError,
  Ping,
  SetServo,
};

enum class Direction : uint8_t {
  None,
  Negative,
  Positive,
};

enum class HolderPosition : uint8_t {
  Unknown,
  Open,
  Closed,
  Service,
};

enum class AxisId : uint8_t {
  Z,
};

struct Command {
  CommandType type = CommandType::None;
  ErrorCode parseError = ErrorCode::None;
  uint32_t id = 0;
  AxisId axis = AxisId::Z;
  int32_t positionSteps = 0;
  uint32_t speedStepsPerSecond = 0;
  uint32_t accelerationStepsPerSecond2 = 0;
  HolderPosition holderPosition = HolderPosition::Unknown;
  bool valid = false;
};

struct SensorSnapshot {
  bool gripperDetected = false;
  bool homeDetected = false;
  bool tofHealthy = true;
  bool frontObstacle = false;
  bool rearObstacle = false;
  uint16_t frontDistanceMm = 0;
  uint16_t rearDistanceMm = 0;
  uint32_t updatedAtMs = 0;
};

struct MotionSnapshot {
  int32_t currentPositionSteps = 0;
  int32_t targetPositionSteps = 0;
  bool busy = false;
  bool homing = false;
  bool moving = false;
  bool referenced = false;
  Direction direction = Direction::None;
};

struct StatusSnapshot {
  AppState state = AppState::Booting;
  ErrorCode error = ErrorCode::None;
  MotionSnapshot motion;
  SensorSnapshot sensors;
};

inline const char *toString(AppState state) {
  switch (state) {
    case AppState::Booting:
      return "BOOTING";
    case AppState::NotReferenced:
      return "NOT_REFERENCED";
    case AppState::Ready:
      return "READY";
    case AppState::BusyHoming:
      return "BUSY_HOMING";
    case AppState::BusyMoving:
      return "BUSY_MOVING";
    case AppState::Stopped:
      return "STOPPED";
    case AppState::Error:
      return "ERROR";
  }

  return "UNKNOWN";
}

inline const char *toString(ErrorCode error) {
  switch (error) {
    case ErrorCode::None:
      return "NONE";
    case ErrorCode::InvalidCommand:
      return "INVALID_COMMAND";
    case ErrorCode::Busy:
      return "BUSY";
    case ErrorCode::NotReferenced:
      return "NOT_REFERENCED";
    case ErrorCode::ObstacleDetected:
      return "OBSTACLE";
    case ErrorCode::MoveTimeout:
      return "MOVE_TIMEOUT";
    case ErrorCode::HomingTimeout:
      return "HOMING_TIMEOUT";
    case ErrorCode::PositionError:
      return "POSITION_ERROR";
    case ErrorCode::SensorFaultTof:
      return "SENSOR_FAULT_TOF";
    case ErrorCode::SensorFaultGripper:
      return "SENSOR_FAULT_GRIPPER";
    case ErrorCode::DriverFault:
      return "DRIVER_FAULT";
    case ErrorCode::CommTimeoutPi:
      return "COMM_TIMEOUT_PI";
    case ErrorCode::InvalidState:
      return "INVALID_STATE";
  }

  return "UNKNOWN";
}

inline const char *toString(CommandType type) {
  switch (type) {
    case CommandType::None:
      return "NONE";
    case CommandType::Status:
      return "STATUS";
    case CommandType::Stop:
      return "STOP";
    case CommandType::Home:
      return "HOME";
    case CommandType::MoveTo:
      return "MOVE_TO";
    case CommandType::ResetError:
      return "RESET_ERROR";
    case CommandType::Ping:
      return "PING";
    case CommandType::SetServo:
      return "SET_SERVO";
  }

  return "UNKNOWN";
}

inline const char *toString(HolderPosition position) {
  switch (position) {
    case HolderPosition::Unknown:
      return "UNKNOWN";
    case HolderPosition::Open:
      return "OPEN";
    case HolderPosition::Closed:
      return "CLOSED";
    case HolderPosition::Service:
      return "SERVICE";
  }

  return "UNKNOWN";
}

inline long absoluteDistance(int32_t left, int32_t right) {
  return left >= right ? static_cast<long>(left - right)
                       : static_cast<long>(right - left);
}

}  // namespace esp_schlitten

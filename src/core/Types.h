#pragma once

#include <Arduino.h>

namespace esp_schlitten {

enum class AppState : uint8_t {
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
  InvalidState,
  ObstacleDetected,
  MoveTimeout,
  HomingTimeout,
  PositionError,
  SensorFaultObstacle,
  SensorFaultGripper,
  DriverFault,
};

enum class CommandType : uint8_t {
  None,
  Ping,
  Status,
  StreamOn,
  StreamOff,
  Stop,
  Home,
  MoveTo,
  SetServo,
  SetDoorArm,
  ResetError,
};

enum class HolderPosition : uint8_t {
  Unknown,
  Open,
  Closed,
  Service,
};

enum class DoorArmPosition : uint8_t {
  Unknown,
  Open,
  Closed,
};

struct Position {
  int32_t x_mm = 0;
  int32_t z_mm = 0;
};

struct Command {
  CommandType type = CommandType::None;
  ErrorCode parseError = ErrorCode::None;
  uint32_t id = 0;
  Position target;
  HolderPosition holderPosition = HolderPosition::Unknown;
  DoorArmPosition doorArmPosition = DoorArmPosition::Unknown;
  bool valid = false;
};

struct SensorSnapshot {
  bool gripperDetected = false;
  bool obstacleOk = true;
  uint16_t doorDistanceMm = 0;
  uint32_t updatedAtMs = 0;
};

struct MotionSnapshot {
  Position current;
  Position target;
  bool busy = false;
  bool referenced = false;
};

struct StatusSnapshot {
  AppState state = AppState::NotReferenced;
  ErrorCode error = ErrorCode::None;
  MotionSnapshot motion;
  SensorSnapshot sensors;
};

inline const char *toString(AppState state) {
  switch (state) {
    case AppState::NotReferenced: return "NOT_REFERENCED";
    case AppState::Ready:         return "READY";
    case AppState::BusyHoming:    return "BUSY_HOMING";
    case AppState::BusyMoving:    return "BUSY_MOVING";
    case AppState::Stopped:       return "STOPPED";
    case AppState::Error:         return "ERROR";
  }
  return "UNKNOWN";
}

inline const char *toString(ErrorCode error) {
  switch (error) {
    case ErrorCode::None:                return "NONE";
    case ErrorCode::InvalidCommand:      return "INVALID_COMMAND";
    case ErrorCode::Busy:                return "BUSY";
    case ErrorCode::NotReferenced:       return "NOT_REFERENCED";
    case ErrorCode::InvalidState:        return "INVALID_STATE";
    case ErrorCode::ObstacleDetected:    return "OBSTACLE";
    case ErrorCode::MoveTimeout:         return "MOVE_TIMEOUT";
    case ErrorCode::HomingTimeout:       return "HOMING_TIMEOUT";
    case ErrorCode::PositionError:       return "POSITION_ERROR";
    case ErrorCode::SensorFaultObstacle: return "SENSOR_FAULT_OBSTACLE";
    case ErrorCode::SensorFaultGripper:  return "SENSOR_FAULT_GRIPPER";
    case ErrorCode::DriverFault:         return "DRIVER_FAULT";
  }
  return "UNKNOWN";
}

inline const char *toString(HolderPosition pos) {
  switch (pos) {
    case HolderPosition::Unknown: return "UNKNOWN";
    case HolderPosition::Open:    return "OPEN";
    case HolderPosition::Closed:  return "CLOSED";
    case HolderPosition::Service: return "SERVICE";
  }
  return "UNKNOWN";
}

inline const char *toString(DoorArmPosition pos) {
  switch (pos) {
    case DoorArmPosition::Unknown: return "UNKNOWN";
    case DoorArmPosition::Open:    return "OPEN";
    case DoorArmPosition::Closed:  return "CLOSED";
  }
  return "UNKNOWN";
}

}  // namespace esp_schlitten

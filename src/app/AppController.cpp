#include "app/AppController.h"

namespace esp_schlitten {

void AppController::begin() {
  Serial.begin(config::Serial::kBaudRate);
  comm_.begin(Serial);
  reporter_.begin(comm_);

  error_ = ErrorCode::None;
  setState(AppState::NotReferenced);
  publishStatus();
}

void AppController::update() {
  const uint32_t nowMs = millis();

  comm_.poll();
  processCommands();

  if (streamEnabled_ && (nowMs - lastStreamAtMs_) >= config::Timing::kStreamIntervalMs) {
    lastStreamAtMs_ = nowMs;
    publishStatus();
  }

  if ((nowMs - lastHeartbeatAtMs_) >= config::Timing::kHeartbeatIntervalMs) {
    lastHeartbeatAtMs_ = nowMs;
    reporter_.sendHeartbeat(state_, motionSnapshot());
  }
}

// ---------------------------------------------------------------------------

void AppController::processCommands() {
  while (comm_.hasPendingCommand()) {
    processCommand(comm_.popCommand());
  }
}

void AppController::processCommand(const Command &cmd) {
  if (!cmd.valid) {
    comm_.sendResponseError(cmd.id, cmd.parseError);
    return;
  }

  switch (cmd.type) {

    case CommandType::Ping:
      comm_.sendAck(cmd.id);
      reporter_.sendOk(cmd.id, "PONG", motionSnapshot());
      return;

    case CommandType::Status:
      comm_.sendAck(cmd.id);
      publishStatus();
      return;

    case CommandType::StreamOn:
      streamEnabled_  = true;
      lastStreamAtMs_ = millis();
      comm_.sendAck(cmd.id);
      reporter_.sendOk(cmd.id, "STREAM_ON", motionSnapshot());
      return;

    case CommandType::StreamOff:
      streamEnabled_ = false;
      comm_.sendAck(cmd.id);
      reporter_.sendOk(cmd.id, "STREAM_OFF", motionSnapshot());
      return;

    case CommandType::Stop: {
      // TODO: Motoren stoppen sobald MotionController vorhanden
      comm_.sendAck(cmd.id);
      if (state_ != AppState::Error) {
        error_ = ErrorCode::None;
        setState(AppState::Stopped);
        reporter_.sendOk(cmd.id, "STOPPED", motionSnapshot());
      }
      return;
    }

    case CommandType::ResetError:
      if (state_ != AppState::Error) {
        comm_.sendResponseError(cmd.id, ErrorCode::InvalidState);
        return;
      }
      error_ = ErrorCode::None;
      comm_.sendAck(cmd.id);
      setState(AppState::NotReferenced);
      reporter_.sendOk(cmd.id, "ERROR_RESET", motionSnapshot());
      return;

    case CommandType::Home:
      handleHome(cmd);
      return;

    case CommandType::MoveTo:
      handleMoveTo(cmd);
      return;

    case CommandType::SetClamp:
      handleSetClamp(cmd);
      return;

    case CommandType::SetDoorArm:
      handleSetDoorArm(cmd);
      return;

    default:
      comm_.sendResponseError(cmd.id, ErrorCode::InvalidCommand);
  }
}

// ---------------------------------------------------------------------------

void AppController::handleHome(const Command &cmd) {
  if (state_ == AppState::Error) {
    comm_.sendResponseError(cmd.id, ErrorCode::InvalidState);
    return;
  }
  if (state_ == AppState::BusyHoming || state_ == AppState::BusyMoving) {
    comm_.sendResponseError(cmd.id, ErrorCode::Busy);
    return;
  }

  comm_.sendAck(cmd.id);
  setState(AppState::BusyHoming);

  // TODO: MotionController.startHoming() aufrufen
  // Stub: sofortige Simulation bis Hardware vorhanden
  current_ = Position{};
  target_  = Position{};
  error_   = ErrorCode::None;
  setState(AppState::Ready);
  reporter_.sendOk(cmd.id, "HOME_DONE", motionSnapshot());
}

void AppController::handleMoveTo(const Command &cmd) {
  if (state_ == AppState::Error) {
    comm_.sendResponseError(cmd.id, ErrorCode::InvalidState);
    return;
  }
  if (state_ != AppState::Ready && state_ != AppState::Stopped) {
    comm_.sendResponseError(cmd.id, ErrorCode::Busy);
    return;
  }
  if (state_ == AppState::NotReferenced) {
    comm_.sendResponseError(cmd.id, ErrorCode::NotReferenced);
    return;
  }

  target_ = cmd.target;
  comm_.sendAck(cmd.id);
  setState(AppState::BusyMoving);

  // TODO: MotionController.startMoveTo() aufrufen
  // Stub: sofortige Simulation bis Hardware vorhanden
  current_ = target_;
  error_   = ErrorCode::None;
  setState(AppState::Ready);
  reporter_.sendOk(cmd.id, "MOVE_DONE", motionSnapshot());
}

void AppController::handleSetClamp(const Command &cmd) {
  if (state_ == AppState::Error) {
    comm_.sendResponseError(cmd.id, ErrorCode::InvalidState);
    return;
  }
  if (state_ == AppState::BusyHoming || state_ == AppState::BusyMoving) {
    comm_.sendResponseError(cmd.id, ErrorCode::Busy);
    return;
  }

  comm_.sendAck(cmd.id);

  // TODO: ClampServoController aufrufen
  String eventName = String("CLAMP_") + toString(cmd.clampPosition);
  reporter_.sendOk(cmd.id, eventName.c_str(), motionSnapshot());
}

void AppController::handleSetDoorArm(const Command &cmd) {
  if (state_ == AppState::Error) {
    comm_.sendResponseError(cmd.id, ErrorCode::InvalidState);
    return;
  }
  if (state_ == AppState::BusyHoming || state_ == AppState::BusyMoving) {
    comm_.sendResponseError(cmd.id, ErrorCode::Busy);
    return;
  }

  comm_.sendAck(cmd.id);

  // TODO: DoorArmController aufrufen (Stepper oder Servo – noch offen)
  String eventName = String("DOOR_ARM_") + toString(cmd.doorArmPosition);
  reporter_.sendOk(cmd.id, eventName.c_str(), motionSnapshot());
}

// ---------------------------------------------------------------------------

void AppController::setState(AppState next) {
  if (state_ == next) return;
  state_ = next;
  reporter_.sendState(state_, motionSnapshot());
}

void AppController::enterError(ErrorCode error) {
  // TODO: Motoren stoppen sobald MotionController vorhanden
  error_ = error;
  setState(AppState::Error);
  reporter_.sendError(error, motionSnapshot());
  publishStatus();
}

void AppController::publishStatus() {
  StatusSnapshot s;
  s.state   = state_;
  s.error   = error_;
  s.motion  = motionSnapshot();
  s.sensors = sensorSnapshot();
  reporter_.sendStatus(s);
}

MotionSnapshot AppController::motionSnapshot() const {
  MotionSnapshot m;
  m.current    = current_;
  m.target     = target_;
  m.busy       = (state_ == AppState::BusyHoming || state_ == AppState::BusyMoving);
  m.referenced = (state_ == AppState::Ready   ||
                  state_ == AppState::BusyMoving ||
                  state_ == AppState::Stopped);
  return m;
}

SensorSnapshot AppController::sensorSnapshot() const {
  // TODO: echte Sensor-Hardware auslesen
  SensorSnapshot s;
  s.obstacleOk    = true;
  s.doorDistanceMm = 0;
  return s;
}

}  // namespace esp_schlitten

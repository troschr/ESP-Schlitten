#include "app/AppController.h"

namespace esp_schlitten {

void AppController::begin() {
  Serial.begin(config::Serial::kBaudRate);
  commandInterface_.begin(Serial);
  statusReporter_.begin(commandInterface_);

  sensorManager_.begin();
  servoController_.begin();
  motionController_.begin();
  safetyManager_.begin(millis());

  error_ = ErrorCode::None;
  setState(AppState::NotReferenced);
  publishStatus();
}

void AppController::update() {
  const uint32_t nowMs = millis();
  const uint32_t nowMicros = micros();

  commandInterface_.poll();
  sensorManager_.update(nowMs);
  processCommands(nowMs);

  const MotionController::UpdateResult motionResult =
      motionController_.update(nowMs, nowMicros, sensorManager_.snapshot());

  if (motionResult.fault != ErrorCode::None) {
    enterError(motionResult.fault);
    return;
  }

  if (motionResult.homeCompleted) {
    error_ = ErrorCode::None;
    setState(AppState::Ready);
    statusReporter_.sendOk(activeCommandId_, "HOME_DONE", motionController_.snapshot());
    activeCommandId_ = 0;
  } else if (motionResult.moveCompleted) {
    error_ = ErrorCode::None;
    setState(AppState::Ready);
    statusReporter_.sendOk(activeCommandId_, "MOVE_DONE", motionController_.snapshot());
    activeCommandId_ = 0;
  }

  const ErrorCode safetyError =
      safetyManager_.update(nowMs, sensorManager_.snapshot(), motionController_.snapshot());
  if (safetyError != ErrorCode::None) {
    enterError(safetyError);
  }
}

void AppController::processCommands(uint32_t nowMs) {
  while (commandInterface_.hasPendingCommand()) {
    const Command command = commandInterface_.popCommand();
    if (command.valid) {
      safetyManager_.notePiContact(nowMs);
    }
    processCommand(command, nowMs);
  }
}

void AppController::processCommand(const Command &command, uint32_t nowMs) {
  if (!command.valid) {
    commandInterface_.sendResponseError(command.id, command.parseError);
    return;
  }

  if (command.type == CommandType::Status) {
    commandInterface_.sendAck(command.id);
    publishStatus();
    return;
  }

  if (command.type == CommandType::Ping) {
    commandInterface_.sendAck(command.id);
    statusReporter_.sendOk(command.id, "PONG", motionController_.snapshot());
    return;
  }

  if (command.type == CommandType::Stop) {
    motionController_.stopImmediate();
    commandInterface_.sendAck(command.id);
    if (state_ != AppState::Error) {
      error_ = ErrorCode::None;
      setState(AppState::Stopped);
      statusReporter_.sendOk(command.id, "STOPPED", motionController_.snapshot());
    }
    activeCommandId_ = 0;
    return;
  }

  if (command.type == CommandType::ResetError) {
    if (state_ != AppState::Error) {
      commandInterface_.sendResponseError(command.id, ErrorCode::InvalidState);
      return;
    }

    motionController_.clearReference();
    error_ = ErrorCode::None;
    commandInterface_.sendAck(command.id);
    setState(AppState::NotReferenced);
    statusReporter_.sendOk(command.id, "ERROR_RESET", motionController_.snapshot());
    return;
  }

  if (command.type == CommandType::SetServo) {
    if (state_ == AppState::Error) {
      commandInterface_.sendResponseError(command.id, ErrorCode::InvalidState);
      return;
    }

    if (motionController_.isBusy()) {
      commandInterface_.sendResponseError(command.id, ErrorCode::Busy);
      return;
    }

    if (!servoController_.setPosition(command.holderPosition)) {
      commandInterface_.sendResponseError(command.id, ErrorCode::InvalidCommand);
      return;
    }

    commandInterface_.sendAck(command.id);
    statusReporter_.sendOk(command.id, String("SERVO_") + String(toString(command.holderPosition)),
                           motionController_.snapshot());
    return;
  }

  if (motionController_.isBusy()) {
    commandInterface_.sendResponseError(command.id, ErrorCode::Busy);
    return;
  }

  if (state_ == AppState::Error) {
    commandInterface_.sendResponseError(command.id, ErrorCode::InvalidState);
    return;
  }

  if (command.type == CommandType::Home) {
    if (!motionController_.startHoming(nowMs)) {
      commandInterface_.sendResponseError(command.id, ErrorCode::Busy);
      return;
    }

    activeCommandId_ = command.id;
    commandInterface_.sendAck(command.id);
    error_ = ErrorCode::None;
    setState(AppState::BusyHoming);
    return;
  }

  if (command.type == CommandType::MoveTo) {
    if (!motionController_.isReferenced()) {
      commandInterface_.sendResponseError(command.id, ErrorCode::NotReferenced);
      return;
    }

    if (!motionController_.startMoveTo(command.positionSteps, command.speedStepsPerSecond,
                                       nowMs)) {
      commandInterface_.sendResponseError(command.id, ErrorCode::Busy);
      return;
    }

    activeCommandId_ = command.id;
    commandInterface_.sendAck(command.id);
    error_ = ErrorCode::None;
    setState(AppState::BusyMoving);
    return;
  }

  commandInterface_.sendResponseError(command.id, ErrorCode::InvalidCommand);
}

void AppController::publishStatus() {
  StatusSnapshot snapshot;
  snapshot.state = state_;
  snapshot.error = error_;
  snapshot.motion = motionController_.snapshot();
  snapshot.sensors = sensorManager_.snapshot();
  statusReporter_.sendStatus(snapshot);
}

void AppController::setState(AppState nextState) {
  if (state_ == nextState) {
    return;
  }

  state_ = nextState;
  statusReporter_.sendState(state_, motionController_.snapshot());
}

void AppController::enterError(ErrorCode error) {
  motionController_.stopImmediate();
  error_ = error;
  activeCommandId_ = 0;
  setState(AppState::Error);
  statusReporter_.sendError(error, motionController_.snapshot());
  publishStatus();
}

}  // namespace esp_schlitten

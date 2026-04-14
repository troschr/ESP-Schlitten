#include "status/StatusReporter.h"

namespace esp_schlitten {

void StatusReporter::begin(CommandInterface &commandInterface) {
  commandInterface_ = &commandInterface;
}

void StatusReporter::sendState(AppState state, const MotionSnapshot &motion) {
  if (commandInterface_ == nullptr) {
    return;
  }

  commandInterface_->sendLine(String("EVT;0;STATE;") + String(toString(state)) + ";ref=" +
                              String(motion.referenced ? 1 : 0) + ";pos=" +
                              String(motion.currentPositionSteps));
}

void StatusReporter::sendStatus(const StatusSnapshot &snapshot) {
  if (commandInterface_ == nullptr) {
    return;
  }

  commandInterface_->sendLine(
      String("EVT;0;STATUS;state=") + String(toString(snapshot.state)) + ";error=" +
      String(toString(snapshot.error)) + ";ref=" + String(snapshot.motion.referenced ? 1 : 0) +
      ";pos=" + String(snapshot.motion.currentPositionSteps) + ";target=" +
      String(snapshot.motion.targetPositionSteps) + ";busy=" +
      String(snapshot.motion.busy ? 1 : 0) + ";gripper=" +
      String(snapshot.sensors.gripperDetected ? 1 : 0) + ";home=" +
      String(snapshot.sensors.homeDetected ? 1 : 0) + ";tof_ok=" +
      String(snapshot.sensors.tofHealthy ? 1 : 0) + ";front_mm=" +
      String(snapshot.sensors.frontDistanceMm) + ";rear_mm=" +
      String(snapshot.sensors.rearDistanceMm));
}

void StatusReporter::sendError(ErrorCode error, const MotionSnapshot &motion) {
  if (commandInterface_ == nullptr) {
    return;
  }

  commandInterface_->sendLine(String("EVT;0;ERR;") + String(toString(error)) + ";pos=" +
                              String(motion.currentPositionSteps) + ";target=" +
                              String(motion.targetPositionSteps));
}

void StatusReporter::sendOk(uint32_t id, const String &eventName, const MotionSnapshot &motion) {
  if (commandInterface_ == nullptr) {
    return;
  }

  commandInterface_->sendLine(String("EVT;") + String(id) + ";OK;" + eventName + ";pos=" +
                              String(motion.currentPositionSteps) + ";target=" +
                              String(motion.targetPositionSteps));
}

}  // namespace esp_schlitten

#include "safety/SafetyManager.h"

namespace esp_schlitten {

void SafetyManager::begin(uint32_t nowMs) {
  lastPiContactAtMs_ = nowMs;
  commWatchdogArmed_ = false;
}

void SafetyManager::notePiContact(uint32_t nowMs) {
  lastPiContactAtMs_ = nowMs;
  commWatchdogArmed_ = true;
}

ErrorCode SafetyManager::update(uint32_t nowMs, const SensorSnapshot &sensors,
                                const MotionSnapshot &motion) const {
  if (!sensors.tofHealthy) {
    return ErrorCode::SensorFaultTof;
  }

  if (commWatchdogArmed_ && motion.busy &&
      (nowMs - lastPiContactAtMs_) > config::Safety::kPiHeartbeatTimeoutMs) {
    return ErrorCode::CommTimeoutPi;
  }

  return ErrorCode::None;
}

}  // namespace esp_schlitten

#pragma once

#include <Arduino.h>

namespace esp_schlitten {
namespace pins {

static constexpr int8_t kAxisStepPin = -1;
static constexpr int8_t kAxisDirPin = -1;
static constexpr int8_t kAxisEnablePin = -1;

static constexpr int8_t kGripperDetectPin = -1;
static constexpr int8_t kHomeDetectPin = -1;
static constexpr int8_t kServoPwmPin = -1;

}  // namespace pins
}  // namespace esp_schlitten

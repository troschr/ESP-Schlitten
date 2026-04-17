#pragma once

#include <Arduino.h>

namespace esp_schlitten {
namespace pins {

// I2C – Türsensor (VL53L0X, bereits getestet)
static constexpr int8_t kI2cSdaPin    = 21;
static constexpr int8_t kI2cSclPin    = 22;
static constexpr int8_t kTofXshutPin  = 16;

// X-Achse (Stepper) – noch nicht verdrahtet
static constexpr int8_t kAxisXStepPin   = -1;
static constexpr int8_t kAxisXDirPin    = -1;
static constexpr int8_t kAxisXEnablePin = -1;
static constexpr int8_t kHomeXPin       = -1;

// Z-Achse (Stepper) – noch nicht verdrahtet
static constexpr int8_t kAxisZStepPin   = -1;
static constexpr int8_t kAxisZDirPin    = -1;
static constexpr int8_t kAxisZEnablePin = -1;
static constexpr int8_t kHomeZPin       = -1;

// Hindernissensor – Typ und Pin noch offen
static constexpr int8_t kObstacleSensorPin = -1;

// Greifer-Taster – noch nicht verdrahtet
static constexpr int8_t kGripperDetectPin = -1;

// Servo Plattenhalter – noch nicht verdrahtet
static constexpr int8_t kServoPwmPin = -1;

// Türarm-Aktor (Stepper oder Servo, noch nicht entschieden) – noch nicht verdrahtet
static constexpr int8_t kDoorArmPin = -1;

}  // namespace pins
}  // namespace esp_schlitten

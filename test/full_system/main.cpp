// Vollsystem-Test: 2x CL42T + 2x DRV8825 + VL53L0X + TF-Luna
//
// Pinbelegung: siehe src/config/Pins.h
//
// Befehle (Seriell 115200):
//   goto <x_mm> <z_mm>         CL42T X+Z verfahren (mm, absolut)
//   goto steps <x_st> <z_st>  CL42T X+Z verfahren (Schritte, relativ; neg. = rückwärts)
//   home all                   Alle 4 Motorachsen auf Nullpunkt
//   home x|z|greifer|tuer      Einzelne Achse homen
//   greifer <mm>               Greifer ±mm relativ (pos. = ausfahren)
//   greifer steps <n>          Greifer ±n Schritte relativ
//   tuer <mm>                  Tür ±mm relativ (pos. = ausfahren)
//   tuer steps <n>             Tür ±n Schritte relativ
//   tuer oeffnen          Greifer aus → Schlitten+Greifer ein+Tür auf
//   all go                goto 500/500 + alle 4 Motoren gleichzeitig
//   read                  Beide Sensoren einmal auslesen
//   stream on|off         Sensor-Streaming (100 ms)
//   scan                  I2C-Bus scannen
//   ?                     Status

#include <Arduino.h>
#include <Wire.h>

#include <VL53L0X.h>
#include "config/Pins.h"

// ════════════════════════════════════════════════════════════════════
// KALIBRIERUNG 
// ════════════════════════════════════════════════════════════════════

// Config::MotionX
constexpr float    X_STEPS_PER_MM  = 5.75f;   // → Config::MotionX::STEPS_PER_MM
constexpr uint32_t X_STEPS_PER_REV = 800;      // → Config::MotionX::STEPS_PER_REV
constexpr uint16_t X_MAX_RPM       = 22;        // → Config::MotionX::MAX_RPM
constexpr uint16_t X_START_RPM     = 2;         // → Config::MotionX::START_RPM
constexpr uint32_t X_ACCEL_STEPS   = 2000;      // → Config::MotionX::ACCEL_STEPS

// Config::MotionZ
constexpr float    Z_STEPS_PER_MM  = 25.0f;    // → Config::MotionZ::STEPS_PER_MM
constexpr uint32_t Z_STEPS_PER_REV = 800;       // → Config::MotionZ::STEPS_PER_REV
constexpr uint16_t Z_MAX_RPM       = 120;        // → Config::MotionZ::MAX_RPM
constexpr uint16_t Z_START_RPM     = 15;          // → Config::MotionZ::START_RPM
constexpr uint32_t Z_ACCEL_STEPS   = 5000;       // → Config::MotionZ::ACCEL_STEPS

// CL42T Timing (Chip-Spec, normalerweise nicht kalibrieren)
constexpr uint32_t CL_T_STEP_US    = 3;
constexpr uint32_t CL_T_DIR_US     = 5;

// Config::Gripper
constexpr float    GRIPPER_STEPS_PER_MM  = 20.0f; // → Config::Gripper::STEPS_PER_MM
constexpr uint32_t GRIPPER_TRAVEL_STEPS  = 800;    // → Config::Gripper::TRAVEL_STEPS
constexpr uint32_t GRIPPER_STEP_DELAY_US = 1800;   // → Config::Gripper::STEP_DELAY_US
constexpr uint32_t GRIPPER_STEP_US       = 4;      // → Config::Gripper::STEP_US
constexpr uint32_t GRIPPER_DIR_US        = 1;      // → Config::Gripper::DIR_US

// Config::DoorArm
constexpr float    DOOR_STEPS_PER_MM     = 20.0f;  // → Config::DoorArm::STEPS_PER_MM
constexpr uint32_t DOOR_TRAVEL_STEPS     = 800;    // → Config::DoorArm::TRAVEL_STEPS
constexpr uint32_t DOOR_STEP_DELAY_US    = 1500;   // → Config::DoorArm::STEP_DELAY_US
constexpr uint32_t DOOR_STEP_US          = 4;      // → Config::DoorArm::STEP_US
constexpr uint32_t DOOR_DIR_US           = 1;      // → Config::DoorArm::DIR_US

// Hilfskonstante für tuer-oeffnen-Sequenz
constexpr float    TUER_SCHLITTEN_MM     = 50.0f;

// ════════════════════════════════════════════════════════════════════
// Sensoren – Konfiguration
// ════════════════════════════════════════════════════════════════════

constexpr uint8_t  TFLUNA_ADDR        = 0x10;
constexpr uint16_t TFLUNA_AMP_MIN     = 100;
constexpr uint32_t STREAM_INTERVAL_MS = 100;

// ════════════════════════════════════════════════════════════════════
// CL42T – Strukturen & Logik
// ════════════════════════════════════════════════════════════════════

constexpr uint32_t rpmToHalfUs(uint16_t rpm, uint32_t stepsPerRev) {
    return 30000000UL / ((uint32_t)rpm * stepsPerRev);
}

struct ClMotor {
    uint8_t  pinStep, pinDir, pinEn;
    uint32_t totalSteps, stepsDone;
    bool     forward, enabled;
    float    positionMm;
    float    stepsMm;
    uint32_t halfUsCruise, halfUsStart, accelSteps;
    uint16_t maxRpm, startRpm;
};

ClMotor axisX = { Pins::X_STEP, Pins::X_DIR, Pins::X_EN, 0, 0, true, false, 0.0f,
                  X_STEPS_PER_MM,
                  rpmToHalfUs(X_MAX_RPM, X_STEPS_PER_REV), rpmToHalfUs(X_START_RPM, X_STEPS_PER_REV),
                  X_ACCEL_STEPS, X_MAX_RPM, X_START_RPM };

ClMotor axisZ = { Pins::Z_STEP, Pins::Z_DIR, Pins::Z_EN, 0, 0, true, false, 0.0f,
                  Z_STEPS_PER_MM,
                  rpmToHalfUs(Z_MAX_RPM, Z_STEPS_PER_REV), rpmToHalfUs(Z_START_RPM, Z_STEPS_PER_REV),
                  Z_ACCEL_STEPS, Z_MAX_RPM, Z_START_RPM };

void initClMotor(const ClMotor& m) {
    pinMode(m.pinStep, OUTPUT); pinMode(m.pinDir, OUTPUT);
    pinMode(m.pinEn,   OUTPUT);
    digitalWrite(m.pinStep, LOW); digitalWrite(m.pinDir, LOW); digitalWrite(m.pinEn, HIGH);
}

void enableClMotor(ClMotor& m, bool on) {
    m.enabled = on;
    digitalWrite(m.pinEn, on ? LOW : HIGH);
}

uint32_t trapezDelay(const ClMotor& m) {
    uint32_t ramp      = min(m.accelSteps, m.totalSteps / 2);
    uint32_t stepsLeft = m.totalSteps - m.stepsDone;
    float t;
    if      (m.stepsDone < ramp) t = (float)m.stepsDone / ramp;
    else if (stepsLeft   <= ramp) t = (float)stepsLeft   / ramp;
    else return m.halfUsCruise;
    float inv = (1.0f / m.halfUsStart) + t * (1.0f / m.halfUsCruise - 1.0f / m.halfUsStart);
    return (uint32_t)(1.0f / inv);
}

void runBothAxes(ClMotor& mx, ClMotor& mz) {
    bool moveX = mx.totalSteps > 0, moveZ = mz.totalSteps > 0;
    if (!moveX && !moveZ) return;
    if (moveX) { enableClMotor(mx, true); digitalWrite(mx.pinDir, mx.forward ? HIGH : LOW); }
    if (moveZ) { enableClMotor(mz, true); digitalWrite(mz.pinDir, mz.forward ? HIGH : LOW); }
    delayMicroseconds(CL_T_DIR_US);
    mx.stepsDone = 0; mz.stepsDone = 0;
    uint32_t now = micros(), nxtX = now, nxtZ = now;
    while ((moveX && mx.stepsDone < mx.totalSteps) || (moveZ && mz.stepsDone < mz.totalSteps)) {
        now = micros();
        if (moveX && mx.stepsDone < mx.totalSteps && (int32_t)(now - nxtX) >= 0) {
            { uint32_t d = trapezDelay(mx); digitalWrite(mx.pinStep, HIGH); delayMicroseconds(CL_T_STEP_US); digitalWrite(mx.pinStep, LOW); mx.stepsDone++; nxtX += 2*d; }
        }
        if (moveZ && mz.stepsDone < mz.totalSteps && (int32_t)(now - nxtZ) >= 0) {
            { uint32_t d = trapezDelay(mz); digitalWrite(mz.pinStep, HIGH); delayMicroseconds(CL_T_STEP_US); digitalWrite(mz.pinStep, LOW); mz.stepsDone++; nxtZ += 2*d; }
        }
    }
    Serial.println("Zielposition erreicht.");
}

// ════════════════════════════════════════════════════════════════════
// DRV8825 – Strukturen & Logik
// ════════════════════════════════════════════════════════════════════

enum class AktorState { EINGEFAHREN, AUSGEFAHREN };

struct Aktor {
    const char* name;
    uint8_t     pinStep, pinDir, pinEn;
    uint32_t    travelSteps;
    float       stepsMm;
    uint32_t    stepDelayUs, stepUs, dirUs;
    AktorState  state;
    float       positionMm;
};

Aktor greifer = { "Greifer", Pins::GRIPPER_STEP, Pins::GRIPPER_DIR, Pins::GRIPPER_EN,
                  GRIPPER_TRAVEL_STEPS, GRIPPER_STEPS_PER_MM,
                  GRIPPER_STEP_DELAY_US, GRIPPER_STEP_US, GRIPPER_DIR_US,
                  AktorState::EINGEFAHREN, 0.0f };

Aktor tuer    = { "Tuer",    Pins::DOOR_STEP,    Pins::DOOR_DIR,    Pins::DOOR_EN,
                  DOOR_TRAVEL_STEPS, DOOR_STEPS_PER_MM,
                  DOOR_STEP_DELAY_US, DOOR_STEP_US, DOOR_DIR_US,
                  AktorState::EINGEFAHREN, 0.0f };

void initAktor(const Aktor& a) {
    pinMode(a.pinStep, OUTPUT); pinMode(a.pinDir, OUTPUT); pinMode(a.pinEn, OUTPUT);
    digitalWrite(a.pinStep, LOW); digitalWrite(a.pinDir, LOW); digitalWrite(a.pinEn, HIGH);
}

void moveAktorSteps(Aktor& a, uint32_t steps, bool ausfahren) {
    if (steps == 0) return;
    digitalWrite(a.pinEn, LOW);
    digitalWrite(a.pinDir, ausfahren ? HIGH : LOW);
    delayMicroseconds(a.dirUs);
    for (uint32_t i = 0; i < steps; i++) {
        digitalWrite(a.pinStep, HIGH); delayMicroseconds(a.stepUs);
        digitalWrite(a.pinStep, LOW);  delayMicroseconds(a.stepDelayUs);
    }
    digitalWrite(a.pinEn, HIGH);
    float delta = (float)steps / a.stepsMm;
    a.positionMm += ausfahren ? delta : -delta;
}

void moveAktor(Aktor& a, bool ausfahren) {
    moveAktorSteps(a, a.travelSteps, ausfahren);
    a.state = ausfahren ? AktorState::AUSGEFAHREN : AktorState::EINGEFAHREN;
    Serial.printf("%s: %s  (Pos: %.2f mm)\n", a.name, ausfahren ? "ausgefahren" : "eingefahren", a.positionMm);
}

void moveAktorMm(Aktor& a, float mm) {
    if (fabsf(mm) < 0.01f) return;
    bool ausfahren = mm > 0.0f;
    uint32_t steps = (uint32_t)(fabsf(mm) * a.stepsMm + 0.5f);
    Serial.printf("%s: %.2f mm → %lu Schritte (%s)\n", a.name, mm, steps, ausfahren ? ">>" : "<<");
    moveAktorSteps(a, steps, ausfahren);
    Serial.printf("%s: Pos %.2f mm\n", a.name, a.positionMm);
}

void runAxesAndAktor(ClMotor& mx, ClMotor& mz,
                     Aktor& a1, bool a1Aus,
                     Aktor* a2 = nullptr, bool a2Aus = false) {
    bool moveX  = mx.totalSteps > 0, moveZ  = mz.totalSteps > 0;
    bool moveA1 = a1.travelSteps > 0, moveA2 = a2 != nullptr && a2->travelSteps > 0;
    if (!moveX && !moveZ && !moveA1 && !moveA2) return;
    if (moveX)  { enableClMotor(mx, true); digitalWrite(mx.pinDir, mx.forward ? HIGH : LOW); }
    if (moveZ)  { enableClMotor(mz, true); digitalWrite(mz.pinDir, mz.forward ? HIGH : LOW); }
    if (moveA1) { digitalWrite(a1.pinEn, LOW);  digitalWrite(a1.pinDir, a1Aus ? HIGH : LOW); }
    if (moveA2) { digitalWrite(a2->pinEn, LOW); digitalWrite(a2->pinDir, a2Aus ? HIGH : LOW); }
    delayMicroseconds(CL_T_DIR_US);
    mx.stepsDone = 0; mz.stepsDone = 0;
    uint32_t a1Done = 0, a2Done = 0;
    uint32_t now = micros(), nxtX = now, nxtZ = now, nxtA1 = now, nxtA2 = now;
    while ((moveX  && mx.stepsDone < mx.totalSteps) || (moveZ  && mz.stepsDone < mz.totalSteps) ||
           (moveA1 && a1Done < a1.travelSteps)      || (moveA2 && a2Done < a2->travelSteps)) {
        now = micros();
        if (moveX && mx.stepsDone < mx.totalSteps && (int32_t)(now - nxtX) >= 0) {
            { uint32_t d = trapezDelay(mx); digitalWrite(mx.pinStep, HIGH); delayMicroseconds(CL_T_STEP_US); digitalWrite(mx.pinStep, LOW); mx.stepsDone++; nxtX += 2*d; }
        }
        if (moveZ && mz.stepsDone < mz.totalSteps && (int32_t)(now - nxtZ) >= 0) {
            { uint32_t d = trapezDelay(mz); digitalWrite(mz.pinStep, HIGH); delayMicroseconds(CL_T_STEP_US); digitalWrite(mz.pinStep, LOW); mz.stepsDone++; nxtZ += 2*d; }
        }
        if (moveA1 && a1Done < a1.travelSteps && (int32_t)(now - nxtA1) >= 0) {
            digitalWrite(a1.pinStep, HIGH); delayMicroseconds(a1.stepUs); digitalWrite(a1.pinStep, LOW);
            a1Done++; nxtA1 += a1.stepDelayUs;
        }
        if (moveA2 && a2Done < a2->travelSteps && (int32_t)(now - nxtA2) >= 0) {
            digitalWrite(a2->pinStep, HIGH); delayMicroseconds(a2->stepUs); digitalWrite(a2->pinStep, LOW);
            a2Done++; nxtA2 += a2->stepDelayUs;
        }
    }
    if (moveA1) { digitalWrite(a1.pinEn,  HIGH); a1.state  = a1Aus ? AktorState::AUSGEFAHREN : AktorState::EINGEFAHREN; }
    if (moveA2) { digitalWrite(a2->pinEn, HIGH); a2->state = a2Aus ? AktorState::AUSGEFAHREN : AktorState::EINGEFAHREN; }
    Serial.println("Zielposition erreicht.");
}

void handleAktor(Aktor& a, const String& arg) {
    if (arg == "home") { a.positionMm = 0.0f; Serial.printf("%s: Nullpunkt gesetzt.\n", a.name); }
    else if (arg.startsWith("steps ")) {
        int32_t n = arg.substring(6).toInt();
        bool ausfahren = n >= 0;
        Serial.printf("%s: %ld Schritte (%s)\n", a.name, n, ausfahren ? ">>" : "<<");
        moveAktorSteps(a, (uint32_t)abs(n), ausfahren);
        Serial.printf("%s: Pos %.2f mm\n", a.name, a.positionMm);
    }
    else {
        float mm = arg.toFloat();
        if (mm != 0.0f || arg == "0" || arg == "0.0") moveAktorMm(a, mm);
        else Serial.printf("ERR: %s home|<mm>|steps <n>\n", a.name);
    }
}

// ════════════════════════════════════════════════════════════════════
// Sensoren – Logik
// ════════════════════════════════════════════════════════════════════

VL53L0X  vl53;
bool     vl53Ok       = false;
bool     tfLunaOk     = false;
bool     streaming    = false;
uint32_t lastStream   = 0;

bool readTFLuna(uint16_t& distCm, uint16_t& amplitude) {
    Wire.beginTransmission(TFLUNA_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)TFLUNA_ADDR, (uint8_t)6) < 6) return false;
    uint8_t b[6];
    for (uint8_t i = 0; i < 6; i++) b[i] = Wire.read();
    distCm    = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
    amplitude = (uint16_t)b[2] | ((uint16_t)b[3] << 8);
    return true;
}

void debugTFLuna() {
    Serial.println("TF-Luna Debug:");
    Wire.beginTransmission(TFLUNA_ADDR);
    uint8_t err = Wire.endTransmission(true);
    Serial.printf("  beginTransmission(0x10): err=%u  %s\n", err,
        err == 0 ? "OK" : err == 2 ? "NACK Adresse" : err == 3 ? "NACK Daten" : "Fehler");
    if (err != 0) { Serial.println("  Sensor nicht erreichbar."); return; }
    Wire.beginTransmission(TFLUNA_ADDR);
    Wire.write(0x00);
    Wire.endTransmission(true);
    delayMicroseconds(200);
    uint8_t n = Wire.requestFrom((uint8_t)TFLUNA_ADDR, (uint8_t)6);
    Serial.printf("  requestFrom: %u Byte erhalten\n", n);
    if (n > 0) {
        Serial.print("  Rohdaten: ");
        for (uint8_t i = 0; i < n; i++) Serial.printf("0x%02X ", Wire.read());
        Serial.println();
    }
    n = Wire.requestFrom((uint8_t)TFLUNA_ADDR, (uint8_t)6);
    Serial.printf("  requestFrom ohne Register-Write: %u Byte\n", n);
    if (n > 0) {
        uint8_t b[6] = {};
        for (uint8_t i = 0; i < n; i++) b[i] = Wire.read();
        Serial.print("  Rohdaten: ");
        for (uint8_t i = 0; i < n; i++) Serial.printf("0x%02X ", b[i]);
        Serial.println();
        uint16_t d = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
        Serial.printf("  → Distanz (interpretiert): %u cm\n", d);
    }
}

void printReading() {
    if (vl53Ok) {
        uint16_t mm = vl53.readRangeContinuousMillimeters();
        if      (vl53.timeoutOccurred()) Serial.print("VL53L0X: TIMEOUT         ");
        else if (mm > 950)               Serial.print("VL53L0X: außer Reichweite");
        else                             Serial.printf("VL53L0X: %5u mm          ", mm);
    } else {
        Serial.print("VL53L0X: n/a             ");
    }
    Serial.print("   |   ");
    if (tfLunaOk) {
        uint16_t cm, amp;
        if (!readTFLuna(cm, amp))       Serial.print("TF-Luna: FEHLER");
        else if (amp < TFLUNA_AMP_MIN)  Serial.printf("TF-Luna: %4u cm  (Amp %u – schwaches Signal)", cm, amp);
        else                            Serial.printf("TF-Luna: %4u cm  (Amp %u)", cm, amp);
    } else {
        Serial.print("TF-Luna: n/a");
    }
    Serial.println();
}

void scanI2C() {
    Serial.println("I2C-Scan...");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X", addr);
            if      (addr == 0x29) Serial.print("  ← VL53L0X");
            else if (addr == 0x10) Serial.print("  ← TF-Luna");
            Serial.println();
            found++;
        }
    }
    Serial.printf(found ? "  %u Gerät(e) gefunden.\n" : "  Keine Geräte gefunden.\n", found);
}

// ════════════════════════════════════════════════════════════════════
// Befehlsverarbeitung & Hauptprogramm
// ════════════════════════════════════════════════════════════════════

void handleTuerOeffnen() {
    Serial.println("Schritt 1: Greifer ausfahren...");
    moveAktor(greifer, true);
    Serial.println("Schritt 2: Schlitten + Greifer ein + Tuer auf...");
    float xZiel = axisX.positionMm + TUER_SCHLITTEN_MM;
    float zZiel = axisZ.positionMm + TUER_SCHLITTEN_MM;
    axisX.totalSteps = (uint32_t)(TUER_SCHLITTEN_MM * axisX.stepsMm + 0.5f);
    axisZ.totalSteps = (uint32_t)(TUER_SCHLITTEN_MM * axisZ.stepsMm + 0.5f);
    axisX.forward = true; axisZ.forward = true;
    runAxesAndAktor(axisX, axisZ, greifer, false, &tuer, true);
    axisX.positionMm = xZiel; axisZ.positionMm = zZiel;
}

void handleAllGo() {
    float dx = 500.0f - axisX.positionMm, dz = 500.0f - axisZ.positionMm;
    axisX.totalSteps = (uint32_t)(fabsf(dx) * axisX.stepsMm + 0.5f);
    axisZ.totalSteps = (uint32_t)(fabsf(dz) * axisZ.stepsMm + 0.5f);
    axisX.forward = dx >= 0.0f; axisZ.forward = dz >= 0.0f;
    uint32_t sG = greifer.travelSteps; AktorState stG = greifer.state;
    uint32_t sT = tuer.travelSteps;    AktorState stT = tuer.state;
    greifer.travelSteps = 5 * 200;
    tuer.travelSteps    = 5 * 200;
    Serial.println("all go: goto 500/500 + Greifer + Tuer je 5 Umdrehungen...");
    runAxesAndAktor(axisX, axisZ, greifer, true, &tuer, true);
    greifer.travelSteps = sG; greifer.state = stG;
    tuer.travelSteps    = sT; tuer.state    = stT;
    axisX.positionMm = 500.0f; axisZ.positionMm = 500.0f;
}

void handleGoto(const String& args) {
    int sp = args.indexOf(' ');
    if (sp < 0) { Serial.println("ERR: goto <x_mm> <z_mm>"); return; }
    float xMm = args.substring(0, sp).toFloat();
    float zMm = args.substring(sp + 1).toFloat();
    float dx = xMm - axisX.positionMm, dz = zMm - axisZ.positionMm;
    axisX.totalSteps = (uint32_t)(fabsf(dx) * axisX.stepsMm + 0.5f);
    axisZ.totalSteps = (uint32_t)(fabsf(dz) * axisZ.stepsMm + 0.5f);
    axisX.forward = dx >= 0.0f; axisZ.forward = dz >= 0.0f;
    Serial.printf("X: %.2f → %.2f mm  (%lu Schritte, %s)\n", axisX.positionMm, xMm, axisX.totalSteps, axisX.forward ? ">>" : "<<");
    Serial.printf("Z: %.2f → %.2f mm  (%lu Schritte, %s)\n", axisZ.positionMm, zMm, axisZ.totalSteps, axisZ.forward ? ">>" : "<<");
    runBothAxes(axisX, axisZ);
    axisX.positionMm = xMm; axisZ.positionMm = zMm;
}

void handleGotoSteps(const String& args) {
    int sp = args.indexOf(' ');
    if (sp < 0) { Serial.println("ERR: goto steps <x_steps> <z_steps>"); return; }
    int32_t xSt = args.substring(0, sp).toInt();
    int32_t zSt = args.substring(sp + 1).toInt();
    axisX.totalSteps = (uint32_t)abs(xSt); axisX.forward = xSt >= 0;
    axisZ.totalSteps = (uint32_t)abs(zSt); axisZ.forward = zSt >= 0;
    Serial.printf("X: %ld Schritte (%s)\n", xSt, axisX.forward ? ">>" : "<<");
    Serial.printf("Z: %ld Schritte (%s)\n", zSt, axisZ.forward ? ">>" : "<<");
    runBothAxes(axisX, axisZ);
    axisX.positionMm += (float)xSt / axisX.stepsMm;
    axisZ.positionMm += (float)zSt / axisZ.stepsMm;
}

void printStatus() {
    Serial.println("────────────────────────────────────────────────────");
    Serial.println("  goto <x_mm> <z_mm>          X+Z verfahren (mm, absolut)");
    Serial.println("  goto steps <x_st> <z_st>   X+Z verfahren (Schritte, relativ)");
    Serial.println("  home all                    Alle 4 Achsen Nullpunkt setzen");
    Serial.println("  home x|z|greifer|tuer       Einzelne Achse Nullpunkt setzen");
    Serial.println("  greifer <mm>                Greifer ±mm relativ (pos. = ausfahren)");
    Serial.println("  greifer steps <n>           Greifer ±n Schritte relativ");
    Serial.println("  tuer <mm>                   Tür ±mm relativ (pos. = ausfahren)");
    Serial.println("  tuer steps <n>              Tür ±n Schritte relativ");
    Serial.println("  tuer oeffnen          Greifer aus → Schlitten+Greifer ein+Tür auf");
    Serial.println("  all go                goto 500/500 + alle 4 Motoren gleichzeitig");
    Serial.println("  read                  Beide Sensoren auslesen");
    Serial.println("  stream on|off         Sensor-Streaming (100 ms)");
    Serial.println("  scan                  I2C-Bus scannen");
    Serial.println("  ?                     Status");
    Serial.println("────────────────────────────────────────────────────");
    Serial.printf("  Pos X: %.2f mm  |  Pos Z: %.2f mm\n", axisX.positionMm, axisZ.positionMm);
    Serial.printf("  X – Max: %u RPM  Start: %u RPM  Rampe: %lu Steps  %.1f Steps/mm\n",
        axisX.maxRpm, axisX.startRpm, axisX.accelSteps, axisX.stepsMm);
    Serial.printf("  Z – Max: %u RPM  Start: %u RPM  Rampe: %lu Steps  %.1f Steps/mm\n",
        axisZ.maxRpm, axisZ.startRpm, axisZ.accelSteps, axisZ.stepsMm);
    Serial.printf("  Greifer: %.2f mm  TravelSteps %lu  %.1f Steps/mm  Delay %lu us\n",
        greifer.positionMm, greifer.travelSteps, greifer.stepsMm, greifer.stepDelayUs);
    Serial.printf("  Tuer:    %.2f mm  TravelSteps %lu  %.1f Steps/mm  Delay %lu us\n",
        tuer.positionMm, tuer.travelSteps, tuer.stepsMm, tuer.stepDelayUs);

    Serial.printf("  SW Greifer-Home: %s  |  SW Tuer-Home: %s\n",
        digitalRead(Pins::GRIPPER_HOME_SW)  == LOW ? "AKTIV" : "offen",
        digitalRead(Pins::DOOR_ARM_HOME_SW) == LOW ? "AKTIV" : "offen");
    Serial.printf("  Platten-Sensor:  %s\n",
        digitalRead(Pins::PLATE_SENSOR) == LOW ? "AKTIV" : "offen");
    Serial.printf("  VL53L0X (0x29): %s\n", vl53Ok   ? "OK" : "nicht gefunden");
    Serial.printf("  TF-Luna (0x10): %s\n", tfLunaOk ? "OK" : "nicht gefunden");
    Serial.printf("  Streaming:      %s\n", streaming ? "EIN" : "AUS");
    Serial.println("────────────────────────────────────────────────────");
}

void handleCommand(const String& raw) {
    String cmd = raw;
    cmd.trim();
    if (cmd.length() == 0) return;

    bool isMotorCmd = cmd.startsWith("goto") || cmd.startsWith("greifer ") ||
                      cmd.startsWith("tuer ") || cmd == "tuer oeffnen" || cmd == "all go";
    bool streamWas = streaming;
    if (isMotorCmd) streaming = false;

    if      (cmd.startsWith("goto steps ")) handleGotoSteps(cmd.substring(11));
    else if (cmd.startsWith("goto "))    handleGoto(cmd.substring(5));
    else if (cmd.startsWith("home")) {
        String arg = cmd.substring(4); arg.trim();
        if      (arg == "all" || arg == "") {
            axisX.positionMm = 0.0f; axisZ.positionMm = 0.0f;
            greifer.state = AktorState::EINGEFAHREN; tuer.state = AktorState::EINGEFAHREN;
            Serial.println("Nullpunkt gesetzt: X, Z, Greifer, Tuer.");
        }
        else if (arg == "x")       { axisX.positionMm = 0.0f;                Serial.println("Nullpunkt gesetzt: X."); }
        else if (arg == "z")       { axisZ.positionMm = 0.0f;                Serial.println("Nullpunkt gesetzt: Z."); }
        else if (arg == "greifer") { greifer.state = AktorState::EINGEFAHREN; greifer.positionMm = 0.0f; Serial.println("Nullpunkt gesetzt: Greifer."); }
        else if (arg == "tuer")    { tuer.state    = AktorState::EINGEFAHREN; tuer.positionMm    = 0.0f; Serial.println("Nullpunkt gesetzt: Tuer."); }
        else                       Serial.println("ERR: home all|x|z|greifer|tuer");
    }
    else if (cmd.startsWith("greifer ")) handleAktor(greifer, cmd.substring(8));
    else if (cmd == "all go")            handleAllGo();
    else if (cmd == "tuer oeffnen")      handleTuerOeffnen();
    else if (cmd.startsWith("tuer "))    handleAktor(tuer, cmd.substring(5));

    if (isMotorCmd) streaming = streamWas;
    else if (cmd == "read")              printReading();
    else if (cmd == "stream on")         { streaming = true;  Serial.println("Streaming EIN."); }
    else if (cmd == "stream off")        { streaming = false; Serial.println("Streaming AUS."); }
    else if (cmd == "scan")              scanI2C();
    else if (cmd == "tfluna debug")      debugTFLuna();
    else if (cmd == "?")                 printStatus();
    else                                 Serial.println("Unbekannt – ? für Hilfe");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    // Endschalter & Plattensensor (active-low, Hardware-Pullup extern)
    pinMode(Pins::GRIPPER_HOME_SW,  INPUT);
    pinMode(Pins::DOOR_ARM_HOME_SW, INPUT);
    pinMode(Pins::PLATE_SENSOR,     INPUT);

    // Motoren
    initClMotor(axisX); initClMotor(axisZ);
    initAktor(greifer); initAktor(tuer);

    // Sensoren
    Wire.begin(Pins::SDA, Pins::SCL);
    vl53.setTimeout(500);
    if (vl53.init()) { vl53.startContinuous(); vl53Ok = true; Serial.println("VL53L0X: OK"); }
    else               Serial.println("VL53L0X: nicht gefunden (0x29)");
    delay(500);
    Wire.beginTransmission(TFLUNA_ADDR);
    tfLunaOk = (Wire.endTransmission() == 0);
    Serial.printf("TF-Luna: %s\n", tfLunaOk ? "OK" : "nicht gefunden (0x10)");

    Serial.println("\nVollsystem-Test (4 Motoren + 2 Sensoren)");
    printStatus();
}

void loop() {
    if (Serial.available()) {
        handleCommand(Serial.readStringUntil('\n'));
    }
    if (!tfLunaOk && millis() > 2000) {
        Wire.beginTransmission(TFLUNA_ADDR);
        if (Wire.endTransmission() == 0) {
            tfLunaOk = true;
            Serial.println("TF-Luna: nachträglich erkannt (0x10)");
        }
    }
    if (streaming && millis() - lastStream >= STREAM_INTERVAL_MS) {
        lastStream = millis();
        printReading();
    }
}

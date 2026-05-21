#include "drivers/DrvActuator.h"
#include <Arduino.h>
#include <soc/gpio_struct.h>   // direkte GPIO-Registeroperationen für ISR
#include <rom/ets_sys.h>       // ets_delay_us – in ROM, immer aus ISR erreichbar

namespace esp_schlitten {

// ─── Statische Instanzverwaltung ──────────────────────────────────────────────

DrvActuator* DrvActuator::_instances[2] = {};

void IRAM_ATTR DrvActuator::_isr0() { if (_instances[0]) _instances[0]->_tick(); }
void IRAM_ATTR DrvActuator::_isr1() { if (_instances[1]) _instances[1]->_tick(); }

// ─── ISR-sichere GPIO-Operationen ────────────────────────────────────────────

static inline void IRAM_ATTR drv_gpioHigh(uint8_t pin) {
    if (pin < 32) GPIO.out_w1ts        = 1UL << pin;
    else          GPIO.out1_w1ts.val   = 1UL << (pin - 32);
}

static inline void IRAM_ATTR drv_gpioLow(uint8_t pin) {
    if (pin < 32) GPIO.out_w1tc        = 1UL << pin;
    else          GPIO.out1_w1tc.val   = 1UL << (pin - 32);
}

// ─── Konstruktor ──────────────────────────────────────────────────────────────

DrvActuator::DrvActuator(uint8_t pinStep, uint8_t pinDir, uint8_t pinEn,
                         uint32_t stepPulseUs, uint32_t dirSetupUs, uint32_t stepDelayUs,
                         uint8_t timerId)
    : _pinStep(pinStep), _pinDir(pinDir), _pinEn(pinEn)
    , _timerId(timerId)
    , _instanceIdx((timerId >= 2) ? timerId - 2 : 0)
    , _stepPulseUs(stepPulseUs), _dirSetupUs(dirSetupUs)
    , _stepDelayUs(stepDelayUs)
{
    _instances[_instanceIdx] = this;
}

// ─── Initialisierung ──────────────────────────────────────────────────────────

void DrvActuator::begin() {
    pinMode(_pinStep, OUTPUT);
    pinMode(_pinDir,  OUTPUT);
    pinMode(_pinEn,   OUTPUT);
    digitalWrite(_pinStep, LOW);
    digitalWrite(_pinDir,  LOW);
    digitalWrite(_pinEn,   HIGH);  // active-low: deaktiviert

    // Prescaler 80 → 1 µs Auflösung
    _timer = timerBegin(_timerId, 80, true);
    timerAttachInterrupt(_timer, _instanceIdx == 0 ? _isr0 : _isr1, true);
    // Alarm wird erst beim ersten move() / startHoming() aktiviert
}

// ─── Bewegungssteuerung ───────────────────────────────────────────────────────

void DrvActuator::move(int32_t steps) {
    move(steps, _stepDelayUs);
}

void DrvActuator::move(int32_t steps, uint32_t stepDelayUs) {
    if (steps == 0) return;
    if (_moving) stop();

    _forward           = steps > 0;
    _stepsLeft         = (uint32_t)abs(steps);
    _activeStepDelayUs = stepDelayUs;

    digitalWrite(_pinEn,  LOW);
    digitalWrite(_pinDir, _forward ? HIGH : LOW);
    delayMicroseconds(_dirSetupUs);

    _moving = true;
    timerWrite(_timer, 0);  // Counter zurücksetzen – sonst startet erster Step erst nach Timer-Überlauf
    timerAlarmWrite(_timer, _activeStepDelayUs, true);
    timerAlarmEnable(_timer);
}

void DrvActuator::startHoming(bool forward, uint32_t stepDelayUs) {
    if (_moving) stop();

    _forward           = forward;
    _homingMode        = true;
    _activeStepDelayUs = (stepDelayUs > 0) ? stepDelayUs : _stepDelayUs;

    digitalWrite(_pinEn,  LOW);
    digitalWrite(_pinDir, _forward ? HIGH : LOW);
    delayMicroseconds(_dirSetupUs);

    _moving = true;
    timerWrite(_timer, 0);
    timerAlarmWrite(_timer, _activeStepDelayUs, true);
    timerAlarmEnable(_timer);
}

bool DrvActuator::update() {
    return !_moving;  // ISR setzt _moving = false bei Abschluss
}

void DrvActuator::stop() {
    // _moving zuerst setzen: bereits gequeuete ISR sieht das Flag und kehrt ab.
    _moving     = false;
    _homingMode = false;
    _stepsLeft  = 0;
    timerAlarmDisable(_timer);
    digitalWrite(_pinEn, HIGH);
}

// ─── ISR ─────────────────────────────────────────────────────────────────────

void IRAM_ATTR DrvActuator::_tick() {
    if (!_moving) return;

    // STEP-Puls erzeugen
    drv_gpioHigh(_pinStep);
    ets_delay_us(_stepPulseUs);
    drv_gpioLow(_pinStep);

    _stepPosition += _forward ? 1 : -1;

    if (!_homingMode) {
        if (_stepsLeft > 0) _stepsLeft--;
        if (_stepsLeft == 0) {
            _moving = false;
            timerAlarmDisable(_timer);
            drv_gpioHigh(_pinEn);  // Treiber deaktivieren
        }
    }
}

}  // namespace esp_schlitten

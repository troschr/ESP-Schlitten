#include "drivers/ClMotor.h"
#include "config/Config.h"
#include <Arduino.h>
#include <soc/gpio_struct.h>   // GPIO.out_w1ts / GPIO.out1_w1ts für ISR-sichere Pinoperationen
#include <rom/ets_sys.h>       // ets_delay_us – in ROM, immer aus ISR erreichbar

namespace esp_schlitten {

// ─── Statische Instanzverwaltung ──────────────────────────────────────────────

ClMotor* ClMotor::_instances[2] = {};

void IRAM_ATTR ClMotor::_isr0() { if (_instances[0]) _instances[0]->_tick(); }
void IRAM_ATTR ClMotor::_isr1() { if (_instances[1]) _instances[1]->_tick(); }

// ─── ISR-sichere GPIO-Operationen (direkte Registeroperationen, kein digitalWrite) ──

static inline void IRAM_ATTR gpioHigh(uint8_t pin) {
    if (pin < 32) GPIO.out_w1ts         = 1UL << pin;       // plain uint32_t
    else          GPIO.out1_w1ts.val    = 1UL << (pin - 32); // union
}

static inline void IRAM_ATTR gpioLow(uint8_t pin) {
    if (pin < 32) GPIO.out_w1tc         = 1UL << pin;       // plain uint32_t
    else          GPIO.out1_w1tc.val    = 1UL << (pin - 32); // union
}

// ─── Konstruktor ──────────────────────────────────────────────────────────────

ClMotor::ClMotor(uint8_t pinStep, uint8_t pinDir, uint8_t pinEn,
                 float stepsPerMm, uint32_t stepsPerRev,
                 uint16_t maxRpm, uint16_t startRpm, uint32_t accelSteps,
                 uint32_t stepPulseUs, uint32_t dirSetupUs,
                 uint8_t timerId)
    : _pinStep(pinStep), _pinDir(pinDir), _pinEn(pinEn)
    , _timerId(timerId & 1u)
    , _stepsPerMm(stepsPerMm), _stepsPerRev(stepsPerRev)
    , _stepPulseUs(stepPulseUs), _dirSetupUs(dirSetupUs)
    , _dfltAccelSteps(accelSteps)
{
    static_assert(Config::MotionX::MAX_RPM   > 0, "MAX_RPM muss > 0 sein");
    static_assert(Config::MotionX::START_RPM > 0, "START_RPM muss > 0 sein");
    static_assert(Config::MotionZ::MAX_RPM   > 0, "MAX_RPM muss > 0 sein");
    static_assert(Config::MotionZ::START_RPM > 0, "START_RPM muss > 0 sein");

    _dfltHalfUsCruise    = _rpmToHalfUs(maxRpm);
    _dfltHalfUsStart     = _rpmToHalfUs(startRpm);
    _instances[_timerId] = this;
}

uint32_t ClMotor::_rpmToHalfUs(uint16_t rpm) const {
    if (rpm == 0) return 0xFFFFFFFFUL;
    return 30000000UL / ((uint32_t)rpm * _stepsPerRev);
}

// ─── Initialisierung ──────────────────────────────────────────────────────────

void ClMotor::begin() {
    pinMode(_pinStep, OUTPUT);
    pinMode(_pinDir,  OUTPUT);
    pinMode(_pinEn,   OUTPUT);
    digitalWrite(_pinStep, LOW);
    digitalWrite(_pinDir,  LOW);
    digitalWrite(_pinEn,   HIGH);  // active-low: deaktiviert

    // Prescaler 80 → 1 µs Auflösung (80 MHz APB-Takt / 80 = 1 MHz Zählfrequenz)
    _timer = timerBegin(_timerId, 80, true);
    timerAttachInterrupt(_timer, _timerId == 0 ? _isr0 : _isr1, true);
    // Alarm wird erst beim ersten moveTo / startHoming aktiviert
}

void ClMotor::enable(bool on) {
    digitalWrite(_pinEn, on ? LOW : HIGH);
}

// ─── Bewegungssteuerung ───────────────────────────────────────────────────────

bool ClMotor::moveTo(float targetMm) {
    return moveTo(targetMm, 0);
}

bool ClMotor::moveTo(float targetMm, uint16_t maxRpm) {
    const int32_t targetSteps =
        (int32_t)(targetMm * _stepsPerMm + (targetMm >= 0 ? 0.5f : -0.5f));
    const int32_t delta = targetSteps - _positionSteps;
    if (delta == 0) return !_moving;

    const bool     newForward   = delta > 0;
    const uint32_t halfUsCruise = (maxRpm > 0) ? _rpmToHalfUs(maxRpm) : _dfltHalfUsCruise;

    if (_moving) {
        // Motor läuft bereits: Ziel und Cruise-Speed anpassen.
        // Atomare 32-bit-Schreibvorgänge – auf ESP32 keine kritische Sektion nötig.
        if (newForward != _forward) return false;  // Richtungswechsel erst nach Stopp
        _halfUsCruise = halfUsCruise;
        _totalSteps   = (uint32_t)abs(delta);
        return false;
    }

    _totalSteps   = (uint32_t)abs(delta);
    _forward      = newForward;
    _homingMode   = false;
    _stepsDone    = 0;
    _halfUsCruise = halfUsCruise;
    _halfUsStart  = _dfltHalfUsStart;
    _accelSteps   = _dfltAccelSteps;

    enable(true);
    digitalWrite(_pinDir, _forward ? HIGH : LOW);
    delayMicroseconds(_dirSetupUs);

    _moving = true;
    timerWrite(_timer, 0);  // Counter zurücksetzen – sonst startet erster Step erst nach Timer-Überlauf
    timerAlarmWrite(_timer, _halfUsStart * 2, true);
    timerAlarmEnable(_timer);
    return false;
}

void ClMotor::startHoming(bool forward, uint16_t homingRpm) {
    if (_moving) stop();

    _forward      = forward;
    _homingMode   = true;
    _totalSteps   = 0xFFFFFFFFUL;
    _stepsDone    = 0;
    _halfUsCruise = _rpmToHalfUs(homingRpm);
    _halfUsStart  = _halfUsCruise;
    _accelSteps   = 0;

    enable(true);
    digitalWrite(_pinDir, _forward ? HIGH : LOW);
    delayMicroseconds(_dirSetupUs);

    _moving = true;
    timerWrite(_timer, 0);
    timerAlarmWrite(_timer, _halfUsCruise * 2, true);
    timerAlarmEnable(_timer);
}

bool ClMotor::update() {
    return !_moving;  // ISR setzt _moving = false bei Abschluss
}

void ClMotor::stop() {
    // _moving zuerst setzen: ISR die schon queued ist sieht das Flag und bricht ab,
    // bevor sie timerAlarmWrite aufruft und den Alarm versehentlich wieder aktiviert.
    _moving     = false;
    _homingMode = false;
    timerAlarmDisable(_timer);
}

void ClMotor::setPositionMm(float mm) {
    _positionSteps = (int32_t)(mm * _stepsPerMm + (mm >= 0 ? 0.5f : -0.5f));
}

float ClMotor::positionMm() const {
    return (int32_t)_positionSteps / _stepsPerMm;
}

// ─── ISR ─────────────────────────────────────────────────────────────────────

void IRAM_ATTR ClMotor::_tick() {
    // Guard: stop() setzt _moving = false vor timerAlarmDisable(); bereits
    // gequeuete ISR-Aufrufe kehren sofort zurück ohne timerAlarmWrite aufzurufen.
    if (!_moving) return;

    // STEP-Puls erzeugen: direkte Registeroperationen, kein digitalWrite in ISR
    gpioHigh(_pinStep);
    ets_delay_us(_stepPulseUs);  // ROM-Funktion, ISR-sicher
    gpioLow(_pinStep);

    if (_forward) _positionSteps++;
    else          _positionSteps--;
    _stepsDone++;

    if (!_homingMode && _stepsDone >= _totalSteps) {
        _moving = false;
        timerAlarmDisable(_timer);
        return;
    }

    // Nächste Step-Periode berechnen und Timer-Alarm neu setzen
    timerAlarmWrite(_timer, _trapezPeriodUs() * 2, true);
}

// Ganzzahl-Trapezrampe – kein Float → keine FPU-Kontextsicherung nötig, ISR-sicher
uint32_t IRAM_ATTR ClMotor::_trapezPeriodUs() const {
    if (_homingMode || _accelSteps == 0) return _halfUsCruise;

    // Rampenlänge: kleiner von accelSteps und halber Gesamtstrecke (symmetrische Rampe)
    const uint32_t ramp      = (_accelSteps < _totalSteps / 2) ? _accelSteps : _totalSteps / 2;
    const uint32_t stepsLeft = _totalSteps - _stepsDone;

    // Welche Rampenphase sind wir gerade in?
    const uint32_t phase = (_stepsDone < ramp)   ? _stepsDone
                         : (stepsLeft  < ramp)   ? stepsLeft
                                                 : ramp;  // Cruise-Phase → ramp = volle Länge

    if (phase >= ramp) return _halfUsCruise;

    // Lineare Interpolation in der Periodendomäne:
    // period = start − (start − cruise) × phase / ramp
    if (_halfUsStart <= _halfUsCruise) return _halfUsCruise;  // ungültige Config abfangen
    const uint32_t delta = ((_halfUsStart - _halfUsCruise) * phase) / ramp;
    return _halfUsStart - delta;
}

}  // namespace esp_schlitten

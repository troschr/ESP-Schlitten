#pragma once
#include <Arduino.h>

// ISR-basierter CL42T Closed-Loop Stepper Driver.
// Jede Instanz bekommt einen eigenen Hardware-Timer (timerId 0 oder 1).
// Steps werden aus dem Timer-ISR erzeugt – unabhängig von der Loop-Auslastung.
// update() dient nur noch zur Abfrageprüfung, erzeugt keine Steps mehr.

namespace esp_schlitten {

class ClMotor {
public:
    // timerId 0 = X-Achse, 1 = Z-Achse
    ClMotor(uint8_t pinStep, uint8_t pinDir, uint8_t pinEn,
            float stepsPerMm, uint32_t stepsPerRev,
            uint16_t maxRpm, uint16_t startRpm, uint32_t accelSteps,
            uint32_t stepPulseUs, uint32_t dirSetupUs,
            uint8_t timerId);

    void begin();
    void enable(bool on);

    // Absolute Zielposition in mm anfahren. false = Bewegung bereits läuft.
    bool moveTo(float targetMm);
    // Wie moveTo(), aber mit explizitem maxRpm für diese Bewegung.
    bool moveTo(float targetMm, uint16_t maxRpm);

    // Homing: Motor fährt mit homingRpm in 'forward'-Richtung bis stop().
    void startHoming(bool forward, uint16_t homingRpm);

    // true = Bewegung abgeschlossen (ISR aktualisiert Zustand asynchron).
    bool update();

    void  stop();
    void  setPositionMm(float mm);
    float positionMm()  const;
    bool  isMoving()    const { return _moving; }

private:
    void     IRAM_ATTR _tick();              // wird aus Timer-ISR aufgerufen
    uint32_t IRAM_ATTR _trapezPeriodUs() const;  // ganzzahlig, kein Float → ISR-sicher
    uint32_t           _rpmToHalfUs(uint16_t rpm) const;

    // Konfiguration
    uint8_t  _pinStep, _pinDir, _pinEn;
    uint8_t  _timerId;
    float    _stepsPerMm;
    uint32_t _stepsPerRev;
    uint32_t _stepPulseUs, _dirSetupUs;
    uint32_t _dfltHalfUsCruise, _dfltHalfUsStart, _dfltAccelSteps;

    // Aktive Bewegungsparameter (werden vor Bewegungsstart gesetzt)
    uint32_t _halfUsCruise = 0;
    uint32_t _halfUsStart  = 0;
    uint32_t _accelSteps   = 0;

    // Bewegungszustand – volatile, da ISR und Haupt-Loop gleichzeitig darauf zugreifen
    volatile bool     _moving        = false;
    volatile bool     _homingMode    = false;
    volatile bool     _forward       = true;
    volatile int32_t  _positionSteps = 0;
    volatile uint32_t _stepsDone     = 0;
    volatile uint32_t _totalSteps    = 0;

    hw_timer_t* _timer = nullptr;

    // Statisches Dispatch-Array für ISR-Callbacks (eine Instanz pro Timer-ID)
    static ClMotor* _instances[2];
    static void IRAM_ATTR _isr0();
    static void IRAM_ATTR _isr1();
};

}  // namespace esp_schlitten

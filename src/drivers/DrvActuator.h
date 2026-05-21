#pragma once
#include <Arduino.h>

// ISR-basierter DRV8825 Stepper Actuator Driver.
// Ansteuerung in Schritten; keine Positionsregelung.
// Jede Instanz bekommt einen eigenen Hardware-Timer (timerId 2 oder 3).
// update() dient nur noch zur Abfrageprüfung, erzeugt keine Steps mehr.

namespace esp_schlitten {

class DrvActuator {
public:
    // timerId 2 = Greifer, 3 = Türarm
    DrvActuator(uint8_t pinStep, uint8_t pinDir, uint8_t pinEn,
                uint32_t stepPulseUs, uint32_t dirSetupUs, uint32_t stepDelayUs,
                uint8_t timerId);

    void begin();

    // Bewegung starten. steps > 0 = vorwärts, steps < 0 = rückwärts.
    void move(int32_t steps);
    // Wie move(), aber mit explizitem Step-Delay (überschreibt Konstruktorwert).
    void move(int32_t steps, uint32_t stepDelayUs);

    // Homing: Motor läuft bis stop() aufgerufen wird.
    // stepDelayUs = 0 → verwendet den im Konstruktor konfigurierten Wert.
    void startHoming(bool forward, uint32_t stepDelayUs = 0);

    // true = Bewegung abgeschlossen (ISR aktualisiert Zustand asynchron).
    bool update();

    void    stop();
    bool    isMoving()      const { return _moving; }
    int32_t stepPosition()  const { return _stepPosition; }
    void    resetPosition()       { _stepPosition = 0; }

private:
    void IRAM_ATTR _tick();  // wird aus Timer-ISR aufgerufen

    uint8_t  _pinStep, _pinDir, _pinEn;
    uint8_t  _timerId;
    uint8_t  _instanceIdx;  // Index in _instances[] (timerId - 2)
    uint32_t _stepPulseUs, _dirSetupUs, _stepDelayUs;

    // ISR-zugänglicher Zustand (volatile)
    volatile uint32_t _activeStepDelayUs = 0;
    volatile int32_t  _stepPosition      = 0;
    volatile uint32_t _stepsLeft         = 0;
    volatile bool     _forward           = true;
    volatile bool     _moving            = false;
    volatile bool     _homingMode        = false;

    hw_timer_t* _timer = nullptr;

    // Statisches Dispatch-Array für ISR-Callbacks (eine Instanz pro Timer-ID)
    static DrvActuator* _instances[2];
    static void IRAM_ATTR _isr0();
    static void IRAM_ATTR _isr1();
};

}  // namespace esp_schlitten

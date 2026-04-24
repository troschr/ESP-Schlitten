// DRV8825 (JOY-IT SBC-MD-DRV) – Testprogramm mit serieller Steuerung
//
// Pinbelegung hier nur für den Test – noch nicht die finale Produktivbelegung!
// M0/M1/M2 werden am Modul hardwired gesetzt (nicht per GPIO gesteuert).
//
// Verkabelung:
//   ESP GPIO 25  -> DRV8825 STEP
//   ESP GPIO 26  -> DRV8825 DIR
//   ESP GPIO 27  -> DRV8825 EN   (LOW = aktiv)
//   GND          -> DRV8825 GND
//   Motor-Netzteil (z.B. 12V) -> VMOT / GND des Moduls
//
// Microstepping: Jumper / M0-M2 auf dem Board setzen (Vollschritt = alle offen)
//
// Befehle (per Serial Monitor, Newline am Ende):
//   f <schritte>   Vorwärts N Schritte         z.B. "f 200"
//   b <schritte>   Rückwärts N Schritte        z.B. "b 400"
//   r <umdr>       Umdrehungen vorwärts         z.B. "r 2"
//   s <us>         Schrittverzögerung setzen    z.B. "s 1000"
//   e              Treiber ein/aus toggeln
//   ?              Hilfe und aktuellen Status

#include <Arduino.h>

// --- Pins ---------------------------------------------------------------
constexpr uint8_t PIN_STEP = 25;
constexpr uint8_t PIN_DIR  = 26;
constexpr uint8_t PIN_EN   = 27;  // active-low

// --- Parameter ----------------------------------------------------------
// Bei Vollschritt (1.8°-Motor) = 200 Schritte/Umdrehung
// Microstepping-Faktor anpassen wenn M0-M2 gesetzt:
//   Full=1, Half=2, 1/4=4, 1/8=8, 1/16=16, 1/32=32
constexpr uint16_t STEPS_PER_REV    = 200;
constexpr uint16_t MICROSTEP_FACTOR = 1;
constexpr uint16_t TOTAL_STEPS      = STEPS_PER_REV * MICROSTEP_FACTOR;

// --- Zustand ------------------------------------------------------------
uint32_t stepDelayUs = 2000;  // µs pro Flanke -> 2000 = ~250 Hz
bool     driverOn    = false;

// --- Hilfsfunktionen ----------------------------------------------------
void enableDriver(bool on) {
    driverOn = on;
    digitalWrite(PIN_EN, on ? LOW : HIGH);
    Serial.printf("Treiber: %s\n", on ? "EIN" : "AUS");
}

void doSteps(uint32_t n, bool forward) {
    if (!driverOn) enableDriver(true);

    digitalWrite(PIN_DIR, forward ? HIGH : LOW);
    delayMicroseconds(10);

    Serial.printf("%s %lu Schritte...\n", forward ? ">>" : "<<", n);
    for (uint32_t i = 0; i < n; i++) {
        digitalWrite(PIN_STEP, HIGH);
        delayMicroseconds(stepDelayUs);
        digitalWrite(PIN_STEP, LOW);
        delayMicroseconds(stepDelayUs);
    }
    Serial.println("fertig.");
}

void printHelp() {
    Serial.println("-------------------------------------------");
    Serial.println("  f <schritte>  Vorwärts");
    Serial.println("  b <schritte>  Rückwärts");
    Serial.println("  r <umdr>      Umdrehungen vorwärts");
    Serial.println("  s <us>        Schrittverzögerung (µs)");
    Serial.println("  e             Treiber ein/aus");
    Serial.println("  ?             Hilfe + Status");
    Serial.println("-------------------------------------------");
    Serial.printf( "  Delay: %lu µs  |  Treiber: %s\n",
                   stepDelayUs, driverOn ? "EIN" : "AUS");
    Serial.printf( "  Schritte/Umdr: %u\n", TOTAL_STEPS);
    Serial.println("-------------------------------------------");
}

void handleCommand(const String& line) {
    String cmd = line;
    cmd.trim();
    if (cmd.length() == 0) return;

    char  op  = cmd.charAt(0);
    long  arg = cmd.length() > 1 ? cmd.substring(2).toInt() : 0;

    switch (op) {
        case 'f':
            if (arg <= 0) { Serial.println("ERR: f <schritte> erwartet"); return; }
            doSteps((uint32_t)arg, true);
            break;

        case 'b':
            if (arg <= 0) { Serial.println("ERR: b <schritte> erwartet"); return; }
            doSteps((uint32_t)arg, false);
            break;

        case 'r':
            if (arg <= 0) { Serial.println("ERR: r <umdr> erwartet"); return; }
            doSteps((uint32_t)arg * TOTAL_STEPS, true);
            break;

        case 's':
            if (arg < 2) { Serial.println("ERR: Minimalwert 2 µs"); return; }
            stepDelayUs = (uint32_t)arg;
            Serial.printf("Schrittverzögerung: %lu µs\n", stepDelayUs);
            break;

        case 'e':
            enableDriver(!driverOn);
            break;

        case '?':
            printHelp();
            break;

        default:
            Serial.printf("Unbekannter Befehl '%c' – ? für Hilfe\n", op);
            break;
    }
}

// --- Setup / Loop -------------------------------------------------------
void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    pinMode(PIN_STEP, OUTPUT);
    pinMode(PIN_DIR,  OUTPUT);
    pinMode(PIN_EN,   OUTPUT);

    digitalWrite(PIN_STEP, LOW);
    digitalWrite(PIN_DIR,  LOW);
    digitalWrite(PIN_EN,   HIGH);

    Serial.println("\nDRV8825 Testprogramm – serielle Steuerung");
    printHelp();
}

void loop() {
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        handleCommand(line);
    }
}

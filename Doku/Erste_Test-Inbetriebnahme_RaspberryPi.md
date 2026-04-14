# Erste Test-Inbetriebnahme mit Raspberry Pi

## Zweck

Der aktuelle ESP-Stand ist nur ein einfacher Test fuer:

- USB-Serial zum Raspberry Pi
- 2x VL53L1X ueber I2C
- Taster an GPIOs mit `INPUT_PULLUP`

Keine Motoren, keine Ablaufsteuerung.

## Verdrahtung

### Raspberry Pi <-> ESP32

- Verbindung per USB
- Baudrate: `115200`
- typischer Port am Pi:
  - `/dev/ttyUSB0`
  - oder `/dev/ttyACM0`

### I2C

- `GPIO21` = `SDA`
- `GPIO22` = `SCL`

### VL53L1X

Beide Sensoren liegen auf demselben I2C-Bus.
Jeder Sensor braucht einen eigenen `XSHUT`-Pin.

- `tof_1`
  - `XSHUT = GPIO16`
  - Adresse nach Init: `0x30`
- `tof_2`
  - `XSHUT = GPIO17`
  - Adresse nach Init: `0x31`

Je Sensor:

- `VIN -> 3V3`
- `GND -> GND`
- `SDA -> GPIO21`
- `SCL -> GPIO22`
- `XSHUT -> jeweiliger GPIO`

### Taster

- `btn_1 = GPIO32`
- `btn_2 = GPIO33`
- `btn_3 = GPIO25`
- `btn_4 = GPIO26`

Je Taster:

- ein Pin an GPIO
- ein Pin an `GND`

Logik:

- offen = `HIGH`
- gedrueckt = `LOW`
- im Status: `1 = gedrueckt`

## Was der Raspberry Pi senden kann

Alle Befehle als Textzeile mit Newline:

- `PING`
- `STATUS`
- `STREAM ON`
- `STREAM OFF`
- `REINIT`
- `I2C_SCAN`
- `HELP`

## Was der ESP sendet

### Beim Start

```text
SENSOR_INIT;name=tof_1;ok=1;addr=0x30
SENSOR_INIT;name=tof_2;ok=1;addr=0x31
READY;fw=esp32_component_test
INFO;commands=PING,STATUS,STREAM ON,STREAM OFF,REINIT,I2C_SCAN,HELP
STATUS;reason=boot;...
```

### Auf `PING`

```text
PONG;uptime_ms=12345
```

### Auf `I2C_SCAN`

```text
I2C_SCAN;found=0x30;found=0x31
```

### Zyklischer Status

```text
STATUS;reason=stream;uptime_ms=...;tof_1_ok=1;tof_1_mm=...;tof_1_timeout=0;tof_2_ok=1;tof_2_mm=...;tof_2_timeout=0;btn_1=0;btn_2=0;btn_3=1;btn_4=0
```

Bedeutung:

- `tof_x_ok=1` Sensor initialisiert
- `tof_x_mm` Distanz in mm
- `tof_x_timeout=1` Timeout beim Lesen
- `btn_x=1` Taster gedrueckt

### Taster-Events

```text
EVENT;type=button;name=btn_1;pressed=1
EVENT;type=button;name=btn_1;pressed=0
```

### Sensor-Timeout-Events

```text
EVENT;type=tof_timeout;name=tof_1;active=1
EVENT;type=tof_timeout;name=tof_1;active=0
```

## Kurzer Testablauf

### 1. Port am Pi finden

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

### 2. Verbinden

Zum Beispiel:

```bash
screen /dev/ttyUSB0 115200
```

### 3. Kommunikation pruefen

Senden:

```text
PING
```

Erwartet:

```text
PONG;uptime_ms=...
```

### 4. I2C pruefen

Senden:

```text
I2C_SCAN
```

Erwartet:

```text
I2C_SCAN;found=0x30;found=0x31
```

### 5. Sensoren pruefen

Senden:

```text
STREAM ON
```

Dann pruefen:

- aendern sich `tof_1_mm` und `tof_2_mm`?
- bleiben `tof_x_timeout=0`?

### 6. Taster pruefen

Dann pruefen:

- offen = `btn_x=0`
- gedrueckt = `btn_x=1`
- zusaetzlich kommen Event-Zeilen beim Umschalten

## Wichtig

- gebaut wird aktuell nur `src/main.cpp`
- `platformio.ini` nutzt dafuer `build_src_filter = +<main.cpp>`
- die Pinbelegung oben ist der aktuelle Teststand

## Erfolgskriterium

Der erste Test ist erfolgreich, wenn:

- `PING` beantwortet wird
- `I2C_SCAN` beide Sensoren findet
- beide VL53L1X Werte liefern
- alle Taster sauber schalten

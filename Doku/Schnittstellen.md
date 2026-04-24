# Schnittstelle ESP32 ↔ Raspberry Pi

## Statuslegende

| Symbol | Bedeutung |
|---|---|
| ✅ | Festgelegt – diese Teile des Protokolls sind final |
| ⚠️ | Noch offen – Mechanismus oder Wert noch nicht entschieden |

---

## ✅ Grundprinzip

- Verbindung: UART over USB
- Baudrate: 115200, 8N1
- Encoding: ASCII, eine Nachricht pro Zeile (`\n` oder `\r\n`)
- Feldtrenner: Semikolon `;`
- Positionsangaben: immer in **Millimeter** (Ganzzahl); Umrechnung in Schritte macht der ESP intern
- Max. Zeilenlänge (Eingang): 160 Zeichen
- Kommando-Queue: 8 Einträge; bei Überlauf kommt sofort `RSP;<id>;ERR;BUSY`

---

## Pi → ESP: Kommandos

Jedes Kommando folgt dem Schema:

```
CMD;<id>;<befehl>[;<key>=<value>...]
```

`<id>` ist eine vom Pi vergebene Ganzzahl ≥ 0. Der ESP spiegelt sie in allen Antworten zurück.

| Kommando | Syntax | Voraussetzung | Status |
|---|---|---|---|
| PING | `CMD;<id>;PING` | immer | ✅ |
| STATUS | `CMD;<id>;STATUS` | immer | ✅ |
| STREAM ON | `CMD;<id>;STREAM_ON` | immer | ✅ |
| STREAM OFF | `CMD;<id>;STREAM_OFF` | immer | ✅ |
| STOP | `CMD;<id>;STOP` | immer | ✅ |
| HOME | `CMD;<id>;HOME` | nicht `ERROR`, nicht busy | ✅ |
| MOVE_TO | `CMD;<id>;MOVE_TO;x=<mm>;z=<mm>` | `READY` oder `STOPPED`, referenziert | ✅ |
| RESET_ERROR | `CMD;<id>;RESET_ERROR` | nur in `ERROR` | ✅ |
| SET_CLAMP | `CMD;<id>;SET_CLAMP;position=<OPEN\|CLOSED\|SERVICE>` | nicht `ERROR`, nicht busy | ⚠️ |
| SET_DOOR_ARM | `CMD;<id>;SET_DOOR_ARM;position=<OPEN\|CLOSED>` | nicht `ERROR`, nicht busy | ⚠️ |

---

### ✅ Parameter MOVE_TO

Beide Achsen werden immer gemeinsam angegeben – der ESP fährt zur angegebenen Zielposition. Wie er dort sicher hinkommt (Reihenfolge der Achsen, Kollisionsvermeidung, Rampen), entscheidet der ESP selbst.

| Feld | Typ | Pflicht | Bedeutung |
|---|---|---|---|
| `x` | int | ja | Zielposition X-Achse in mm |
| `z` | int | ja | Zielposition Z-Achse in mm |

Beispiel:
```
CMD;42;MOVE_TO;x=350;z=120
```

> ⚠️ **Noch offen:** Achsreihenfolge – fährt der ESP beide Achsen gleichzeitig oder erst Z dann X (o.ä.)?  
> ⚠️ **Noch offen:** Positionstoleranz – welche Abweichung in mm ist beim `MOVE_DONE` noch akzeptabel?

---

### ⚠️ Parameter SET_DOOR_ARM

> **Status: noch offen** – Der genaue Mechanismus zum Öffnen der Druckertür ist noch nicht entschieden (Servo, Stepper, Hebelarm o.ä.). Das Kommando und seine logischen Zustände sind als Platzhalter definiert, können sich aber noch ändern.

Der Schlitten trägt einen Arm zum Öffnen der Druckertüren. Der Pi gibt nur die logische Position vor.

| Wert | Bedeutung |
|---|---|
| `OPEN` | Arm ausgefahren, Tür geöffnet |
| `CLOSED` | Arm eingefahren, Tür geschlossen |

Der typische Ablauf am Drucker (vorläufig):
1. Pi schickt `SET_DOOR_ARM;position=OPEN`
2. ESP fährt Arm aus
3. Pi fragt per `STATUS` das Feld `door_open` ab → ESP hat Schwellwert intern ausgewertet
4. Nach Entnahme: Pi schickt `SET_DOOR_ARM;position=CLOSED`

---

### ⚠️ Parameter SET_CLAMP (Halteservo)

> **Status: noch offen** – Ob der Schlitten eine Klemmvorrichtung bekommt, wie sie ausgeführt wird (Servo, Pneumatik, Magnet o.ä.) und ob sie direkt per Kommando oder implizit durch einen ESP-internen Ablauf gesteuert wird, ist noch nicht entschieden.

| Wert | Pulsbreite (vorläufig) | Bedeutung |
|---|---|---|
| `OPEN` | 2000 µs | Plattenhalter geöffnet |
| `CLOSED` | 1000 µs | Plattenhalter geschlossen |
| `SERVICE` | 1500 µs | Mittelstellung für Wartung |

---

## ✅ ESP → Pi: Antworten

### RSP – Sofortantwort auf ein Kommando

Kommt immer direkt nach Empfang eines Kommandos, bevor die Aktion abgeschlossen ist.

```
RSP;<id>;ACK
RSP;<id>;ERR;<fehlercode>
```

`ACK` = Kommando akzeptiert, Aktion läuft.  
`ERR` = Kommando abgelehnt, Aktion **nicht** gestartet.

### EVT – Ereignisse und Abschlussmeldungen

Events kommen asynchron. Der Pi muss sie unabhängig von seinem Sendezustand verarbeiten können.

```
EVT;<id>;OK;<event_name>;x=<mm>;z=<mm>
EVT;0;STATE;<zustandscode>;ref=<0|1>;x=<mm>;z=<mm>
EVT;0;STATUS;state=<z>;error=<e>;ref=<0|1>;x=<mm>;z=<mm>;target_x=<mm>;target_z=<mm>;busy=<0|1>;gripper=<0|1>;home=<0|1>;obstacle_ok=<0|1>;door_open=<0|1>;door_dist_mm=<mm>
EVT;0;ERR;<fehlercode>;x=<mm>;z=<mm>
EVT;0;HEARTBEAT;uptime_ms=<ms>;state=<zustandscode>;x=<mm>;z=<mm>
```

#### EVT OK – Abschluss einer Aktion

`<id>` ist die ID des auslösenden Kommandos.

| event_name | Auslöser | Status |
|---|---|---|
| `PONG` | Antwort auf PING | ✅ |
| `HOME_DONE` | Referenzfahrt erfolgreich | ✅ |
| `MOVE_DONE` | Zielposition erreicht | ✅ |
| `STOPPED` | STOP ausgeführt | ✅ |
| `ERROR_RESET` | RESET_ERROR ausgeführt | ✅ |
| `STREAM_ON` | Stream eingeschaltet | ✅ |
| `STREAM_OFF` | Stream ausgeschaltet | ✅ |
| `CLAMP_OPEN` | Halteservo geöffnet (Platte freigegeben) | ⚠️ |
| `CLAMP_CLOSED` | Halteservo geschlossen (Platte geklemmt) | ⚠️ |
| `CLAMP_SERVICE` | Halteservo in Mittelstellung | ⚠️ |
| `DOOR_ARM_OPEN` | Türarm ausgefahren | ⚠️ |
| `DOOR_ARM_CLOSED` | Türarm eingefahren | ⚠️ |

#### ✅ EVT STATE – Zustandsübergang

Spontan bei jedem Zustandswechsel. ID ist immer `0`.

#### ✅ EVT STATUS – Vollständiger Snapshot

Auf `CMD;<id>;STATUS`, bei jedem Eintritt in `ERROR` und periodisch wenn Stream aktiv. ID ist immer `0`.

| Feld | Bedeutung | Status |
|---|---|---|
| `state` | Aktueller Zustand | ✅ |
| `error` | Aktiver Fehlercode (`NONE` wenn kein Fehler) | ✅ |
| `ref` | `1` = referenziert | ✅ |
| `x` | Ist-Position X in mm | ✅ |
| `z` | Ist-Position Z in mm | ✅ |
| `target_x` | Soll-Position X in mm | ✅ |
| `target_z` | Soll-Position Z in mm | ✅ |
| `busy` | `1` = Bewegung aktiv | ✅ |
| `gripper` | `1` = Greifer-Taster aktiv (Platte erkannt) | ⚠️ |
| `home` | `1` = Home-Taster aktiv | ✅ |
| `obstacle_ok` | `1` = Hindernissensor gesund und frei | ✅ |
| `door_open` | `1` = Tür offen (Entscheidung trifft der ESP intern) | ✅ |
| `door_dist_mm` | Rohwert des Türsensors in mm (für Debugging; nur valide wenn Schlitten an Druckerposition steht) | ✅ |

#### ✅ EVT ERR – Fehler

Bei jedem Fehler der den ESP in `ERROR` versetzt. ID ist immer `0`.  
Danach folgt immer ein `EVT;0;STATUS`.

#### EVT HEARTBEAT – Lebenszeichen des ESP

Das Format ist festgelegt. Das Intervall ist noch offen.

> ⚠️ **Noch offen:** Heartbeat-Intervall – wie oft soll der ESP den Heartbeat senden?

---

## ✅ Zustandscodes

| Code | Bedeutung |
|---|---|
| `NOT_REFERENCED` | Bereit, aber noch keine Referenzfahrt |
| `READY` | Referenziert, wartet auf Kommando |
| `BUSY_HOMING` | Referenzfahrt läuft |
| `BUSY_MOVING` | Fahrt zu Zielposition läuft |
| `STOPPED` | Bewegung per STOP angehalten |
| `ERROR` | Fehler, alle Motoren gestoppt, wartet auf RESET_ERROR |

---

## ✅ Fehlercodes

| Code | Bedeutung |
|---|---|
| `INVALID_COMMAND` | Unbekanntes oder fehlerhaftes Kommando |
| `BUSY` | ESP ist gerade beschäftigt |
| `NOT_REFERENCED` | MOVE_TO ohne vorherige Referenzfahrt |
| `INVALID_STATE` | Kommando im falschen Zustand |
| `OBSTACLE` | Hindernissensor unterschreitet Stoppabstand während Fahrt |
| `MOVE_TIMEOUT` | Zielposition nicht innerhalb des Timeouts erreicht |
| `HOMING_TIMEOUT` | Referenzfahrt nicht innerhalb des Timeouts abgeschlossen |
| `POSITION_ERROR` | Rücklese-Position außerhalb Toleranz |
| `SENSOR_FAULT_OBSTACLE` | Hindernissensor ausgefallen oder nicht initialisierbar |
| `SENSOR_FAULT_GRIPPER` | Greifer-Taster antwortet nicht erwartungsgemäß |
| `DRIVER_FAULT` | Stepper-Treiber meldet Fehler |

---

## ✅ Zustandsübergänge

```
                  ┌──────────────────────────────────────────────┐
                  │                   STOP                        │
                  ▼                                               │
NOT_REFERENCED ──HOME──► BUSY_HOMING ──HOME_DONE──► READY ──MOVE_TO──► BUSY_MOVING
      ▲                       │                      │  ▲               │
      │                       │ Fehler               │  └───MOVE_DONE───┘
      │                       ▼                      │
      │                     ERROR ◄────Fehler─────────┘
      │                       │
      └────RESET_ERROR─────────┘
```

- `STOP` ist aus **jedem** Zustand möglich → landet in `STOPPED`
- `STOPPED` verhält sich wie `READY` für `HOME` und `MOVE_TO`
- `RESET_ERROR` → `NOT_REFERENCED`, Referenzierung gelöscht → danach `HOME` Pflicht
- Jeder Fehler während Homing oder Bewegung → sofort `ERROR`, Motoren gestoppt

---

## ✅ Kommunikationsablauf: typische Sequenzen

### Boot

```
← EVT;0;STATE;NOT_REFERENCED;ref=0;x=0;z=0
← EVT;0;STATUS;state=NOT_REFERENCED;error=NONE;ref=0;x=0;z=0;...
```

### Referenzfahrt

```
→ CMD;1;HOME
← RSP;1;ACK
← EVT;0;STATE;BUSY_HOMING;ref=0;x=0;z=0
← EVT;1;OK;HOME_DONE;x=0;z=0
← EVT;0;STATE;READY;ref=1;x=0;z=0
```

### Zielposition anfahren

```
→ CMD;2;MOVE_TO;x=350;z=120
← RSP;2;ACK
← EVT;0;STATE;BUSY_MOVING;ref=1;x=0;z=0
← EVT;2;OK;MOVE_DONE;x=350;z=120
← EVT;0;STATE;READY;ref=1;x=350;z=120
```

### Türprüfung nach Anfahren

```
→ CMD;3;STATUS
← RSP;3;ACK
← EVT;0;STATUS;state=READY;...;door_open=1;door_dist_mm=312
  (ESP hat Schwellwert intern ausgewertet → door_open=1 bedeutet Tür offen)
```

### Fehlerfall: Hindernis

```
→ CMD;4;MOVE_TO;x=500;z=120
← RSP;4;ACK
← EVT;0;STATE;BUSY_MOVING;ref=1;x=350;z=120
← EVT;0;ERR;OBSTACLE;x=412;z=120
← EVT;0;STATE;ERROR;ref=1;x=412;z=120
← EVT;0;STATUS;state=ERROR;error=OBSTACLE;...
→ CMD;5;RESET_ERROR
← RSP;5;ACK
← EVT;5;OK;ERROR_RESET;x=0;z=0
← EVT;0;STATE;NOT_REFERENCED;ref=0;x=0;z=0
```

### Heartbeat

```
← EVT;0;HEARTBEAT;uptime_ms=45231;state=READY;x=350;x=120
← EVT;0;HEARTBEAT;uptime_ms=46231;state=READY;x=350;z=120
```

---

## Sensoren

### ✅ Türsensor (VL53L0X, aktuell verbaut)

- Sitzt fest am Schlitten
- Wird **nur ausgewertet, wenn der Schlitten die Zielposition erreicht hat** (nicht während der Fahrt)
- Zweck: Prüfen ob die Druckertür wirklich geöffnet ist
- Der ESP trifft die Entscheidung „Tür offen ja/nein" selbst anhand eines internen Schwellwerts
- Im STATUS-Feld `door_open` liefert der ESP das Ergebnis als Boolean; `door_dist_mm` ist zusätzlich als Rohwert für Debugging enthalten

> ⚠️ **Noch offen:** Schwellwert – intern im ESP, noch zu bestätigen (~200 mm)

### ✅ Hindernissensor (Fahrtrichtung) – TF-Luna LiDAR

- Sensor: **TF-Luna** (I2C-LiDAR)
- Hängt am gleichen I2C-Bus wie der Türsensor (VL53L0X)
- Sitzt am Schlitten, zeigt in Fahrtrichtung
- Wird **während der Fahrt** ausgewertet
- Löst bei Unterschreitung des Stoppabstands sofort `ERROR;OBSTACLE` aus
- Richtungsabhängig: bei X-Bewegung relevant, bei Z ggf. anders (abhängig von Montageposition)

> ⚠️ **Noch offen:** Montageposition am Schlitten, Stoppabstand und Warnabstand in mm, I2C-Adresse (Default: 0x10)

---

## Motoransteuerung (CL42T-V41 Closed-Loop-Treiber)

Die Schlittenachsen (X, Z) werden jeweils über einen **STEPPERONLINE CL42T-V41** angesteuert. Das ist ein Closed-Loop-Stepper-Treiber: Die Positionsregelung läuft intern im Treiber (Encoder-Rückkopplung), der ESP gibt nur Schritt-Impulse vor.

### ✅ Elektrische Kenndaten

| Parameter | Wert |
|---|---|
| Versorgungsspannung | 24–48 V DC (separates Netzteil, nicht vom ESP) |
| Ausgangsstrom | 0–3,0 A (einstellbar per DIP) |
| Motorkompatibilität | NEMA 11, 14, 17 mit Inkremental-Encoder (1000 Pulse/U) |

### ✅ Signalschnittstelle ESP32 → CL42T

| Signal | Richtung | Pegel | Beschreibung |
|---|---|---|---|
| STEP (PUL+) | ESP → Treiber | 3,3 V | Schritt-Impuls; 1 Impuls = 1 Mikro-Schritt |
| DIR (DIR+) | ESP → Treiber | 3,3 V | Fahrtrichtung; vor STEP min. 5 µs stabil |
| ENA (ENA+) | ESP → Treiber | 3,3 V | Enable; active-low (LOW = Treiber aktiv, Spule bestromt) |
| ALM | Treiber → ESP | 3,3 V | Alarm-Ausgang; active-low bei Treiberfehler → ESP löst `DRIVER_FAULT` aus |

> Die CL42T-Eingänge sind optoentkoppelt. Minus-Seite (PUL−/DIR−/ENA−) liegt auf GND des ESP. 3,3-V-Pegel sind kompatibel.

### ✅ Timing-Grenzen (aus Datenblatt)

| Parameter | Wert |
|---|---|
| Max. Schrittfrequenz | 200 kHz |
| Min. Pulsbreite STEP | 2,5 µs |
| DIR-Setup-Zeit vor STEP | ≥ 5 µs |

### ⚠️ Mikro-Schritt und Schritte/mm

Der Mikro-Schritt-Divisor wird per DIP-Schalter am Treiber eingestellt (800–51.200 Schritte/U). Die konkrete Einstellung und damit die **Schritte/mm** hängen von Motortyp und Mechanik (Spindel-Steigung, Riemenübersetzung) ab und sind noch festzulegen.

> ⚠️ **Noch offen:** Mikro-Schritt-Einstellung und Schritte/mm für X- und Z-Achse.

### ✅ Closed-Loop-Verhalten

Der CL42T regelt Positions-Folgefehler selbst. Übersteigt der Fehler den internen Schwellwert (z. B. bei Blockade), setzt der Treiber den ALM-Ausgang aktiv. Der ESP wertet ALM aus und wechselt in den Zustand `ERROR` mit Fehlercode `DRIVER_FAULT`. Eine erneute Referenzfahrt (HOME) ist danach Pflicht.

---

## Hardwarebelegung ESP32

→ siehe [Pinbelegung_ESP32.md](Pinbelegung_ESP32.md)

---

## Timing und Parameter

| Parameter | Wert | Status |
|---|---|---|
| Baud Rate | 115200 | ✅ |
| Max. Schrittfrequenz (CL42T) | 200 kHz | ✅ |
| Min. Pulsbreite STEP | 2,5 µs | ✅ |
| Versorgungsspannung Treiber | 24–48 V DC | ✅ |
| Mikro-Schritt-Einstellung | noch offen | ⚠️ |
| Schritte/mm (X-Achse) | noch offen | ⚠️ |
| Schritte/mm (Z-Achse) | noch offen | ⚠️ |
| Türsensor Schwelle „offen" | ~200 mm | ⚠️ noch zu bestätigen |
| Heartbeat-Intervall | noch offen | ⚠️ |
| Stream-Intervall | noch festzulegen | ⚠️ |
| Hindernissensor Stoppabstand | noch offen | ⚠️ |
| Hindernissensor Warnabstand | noch offen | ⚠️ |
| Homing-Timeout | noch festzulegen | ⚠️ |
| Bewegungs-Timeout | noch festzulegen | ⚠️ |
| Positionstoleranz | noch festzulegen | ⚠️ |

---

## Offene Punkte (Zusammenfassung)

| Thema | Frage |
|---|---|
| Motortyp X / Z | Welcher NEMA-17-Motor (mit Encoder) wird verbaut? Spezifikation notwendig für Strom-Einstellung am CL42T. |
| Mechanik X / Z | Spindelsteigung oder Riemenübersetzung → bestimmt Schritte/mm |
| Mikro-Schritt-Einstellung | Welcher DIP-Wert am CL42T (800–51.200 Schritte/U)? |
| GPIO-Belegung (STEP/DIR/ENA/ALM) | Konkrete ESP32-Pins für X- und Z-Treiber noch nicht festgelegt |
| Türöffnungs-Mechanismus | Wie wird die Druckertür geöffnet? Servo, Stepper, Hebel? Aktor am Schlitten? |
| Platten-Entnahme | Wie wird die Platte gegriffen und aus dem Drucker gezogen? |
| Platten-Fixierung | Wie wird die Platte auf dem Schlitten gehalten? Servo-Klemme, Magnet, Vakuum? |
| SET_CLAMP-Steuerung | Direktes Kommando vom Pi, oder implizit durch ESP-internen Ablauf? |
| Türarm-Aktor | Stepper oder Servo? (beeinflusst interne ESP-Implementierung, nicht das Protokoll) |
| Hindernissensor Montage | TF-Luna festgelegt (I2C) – Montageposition und Abstände noch offen |
| Achsreihenfolge bei MOVE_TO | Fährt der ESP immer beide Achsen gleichzeitig, oder erst Z dann X (o.ä.)? |
| Positionstoleranz | Welche Abweichung in mm ist beim MOVE_DONE noch akzeptabel? |
| Heartbeat-Intervall | Wie oft soll der ESP den Heartbeat senden? |
| Türsensor-Schwelle | Welche Distanz gilt als „Tür offen"? (~200 mm, noch zu bestätigen) |
| Positionsformat | Reichen ganzzahlige mm, oder wird 0,1 mm Auflösung benötigt? |

# Schnittstelle ESP32 ↔ Raspberry Pi

## Grundprinzip

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

| Kommando | Syntax | Voraussetzung |
|---|---|---|
| PING | `CMD;<id>;PING` | immer |
| STATUS | `CMD;<id>;STATUS` | immer |
| STREAM ON | `CMD;<id>;STREAM_ON` | immer |
| STREAM OFF | `CMD;<id>;STREAM_OFF` | immer |
| STOP | `CMD;<id>;STOP` | immer |
| HOME | `CMD;<id>;HOME` | nicht `ERROR`, nicht busy |
| MOVE_TO | `CMD;<id>;MOVE_TO;x=<mm>;z=<mm>` | `READY` oder `STOPPED`, referenziert |
| SET_CLAMP | `CMD;<id>;SET_CLAMP;position=<OPEN\|CLOSED\|SERVICE>` | nicht `ERROR`, nicht busy |
| SET_DOOR_ARM | `CMD;<id>;SET_DOOR_ARM;position=<OPEN\|CLOSED>` | nicht `ERROR`, nicht busy |
| RESET_ERROR | `CMD;<id>;RESET_ERROR` | nur in `ERROR` |

### Parameter MOVE_TO

Beide Achsen werden immer gemeinsam angegeben – der ESP fährt zur angegebenen Zielposition. Wie er dort sicher hinkommt (Reihenfolge der Achsen, Kollisionsvermeidung, Rampen), entscheidet der ESP selbst.

| Feld | Typ | Pflicht | Bedeutung |
|---|---|---|---|
| `x` | int | ja | Zielposition X-Achse in mm |
| `z` | int | ja | Zielposition Z-Achse in mm |

Beispiel:
```
CMD;42;MOVE_TO;x=350;z=120
```

### Parameter SET_DOOR_ARM

Der Schlitten trägt einen zusätzlichen Arm zum Öffnen der Druckertüren. Steuert den Halteservo, der die aufgenommene Platte auf dem Schlitten fixiert. Der Pi gibt nur die logische Position vor.

| Wert | Bedeutung |
|---|---|
| `OPEN` | Arm ausgefahren, Tür geöffnet |
| `CLOSED` | Arm eingefahren, Tür geschlossen |

Der typische Ablauf am Drucker:
1. Pi schickt `SET_DOOR_ARM;position=OPEN`
2. ESP fährt Arm aus
3. Pi fragt per `STATUS` den `door_dist_mm`-Wert ab → prüft ob Tür wirklich offen ist
4. Nach Entnahme: Pi schickt `SET_DOOR_ARM;position=CLOSED`

### Parameter SET_CLAMP (Halteservo)

| Wert | Pulsbreite | Bedeutung |
|---|---|---|
| `OPEN` | 2000 µs | Plattenhalter geöffnet |
| `CLOSED` | 1000 µs | Plattenhalter geschlossen |
| `SERVICE` | 1500 µs | Mittelstellung für Wartung |

> Ob der Servo direkt über dieses Kommando gesteuert wird oder implizit durch den Entnahmeprozess, ist noch offen.

---

## ESP → Pi: Antworten

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
EVT;0;STATUS;state=<z>;error=<e>;ref=<0|1>;x=<mm>;z=<mm>;target_x=<mm>;target_z=<mm>;busy=<0|1>;gripper=<0|1>;home=<0|1>;obstacle_ok=<0|1>;door_dist_mm=<mm>
EVT;0;ERR;<fehlercode>;x=<mm>;z=<mm>
EVT;0;HEARTBEAT;uptime_ms=<ms>;state=<zustandscode>;x=<mm>;z=<mm>
```

#### EVT OK – Abschluss einer Aktion

`<id>` ist die ID des auslösenden Kommandos.

| event_name | Auslöser |
|---|---|
| `PONG` | Antwort auf PING |
| `HOME_DONE` | Referenzfahrt erfolgreich |
| `MOVE_DONE` | Zielposition erreicht |
| `STOPPED` | STOP ausgeführt |
| `ERROR_RESET` | RESET_ERROR ausgeführt |
| `CLAMP_OPEN` | Halteservo geöffnet (Platte freigegeben) |
| `CLAMP_CLOSED` | Halteservo geschlossen (Platte geklemmt) |
| `CLAMP_SERVICE` | Halteservo in Mittelstellung |
| `DOOR_ARM_OPEN` | Türarm ausgefahren |
| `DOOR_ARM_CLOSED` | Türarm eingefahren |
| `STREAM_ON` | Stream eingeschaltet |
| `STREAM_OFF` | Stream ausgeschaltet |

#### EVT STATE – Zustandsübergang

Spontan bei jedem Zustandswechsel. ID ist immer `0`.

#### EVT STATUS – Vollständiger Snapshot

Auf `CMD;<id>;STATUS`, bei jedem Eintritt in `ERROR` und periodisch wenn Stream aktiv. ID ist immer `0`.

| Feld | Bedeutung |
|---|---|
| `state` | Aktueller Zustand |
| `error` | Aktiver Fehlercode (`NONE` wenn kein Fehler) |
| `ref` | `1` = referenziert |
| `x` | Ist-Position X in mm |
| `z` | Ist-Position Z in mm |
| `target_x` | Soll-Position X in mm |
| `target_z` | Soll-Position Z in mm |
| `busy` | `1` = Bewegung aktiv |
| `gripper` | `1` = Greifer-Taster aktiv (Platte erkannt) |
| `home` | `1` = Home-Taster aktiv |
| `obstacle_ok` | `1` = Hindernissensor gesund und frei |
| `door_dist_mm` | Messwert des Türsensors in mm (nur valide wenn Schlitten an Druckerposition steht) |

#### EVT ERR – Fehler

Bei jedem Fehler der den ESP in `ERROR` versetzt. ID ist immer `0`.  
Danach folgt immer ein `EVT;0;STATUS`.

#### EVT HEARTBEAT – Lebenszeichen des ESP

Der ESP sendet periodisch einen Heartbeat an den Pi (Intervall: noch festzulegen).  
Der Pi kann darüber erkennen, ob der ESP noch läuft.

---

## Sensoren

### Hindernissensor (Fahrtrichtung)

- Sitzt am Schlitten, zeigt in Fahrtrichtung
- Größere Reichweite als der Türsensor
- Wird **während der Fahrt** ausgewertet
- Löst bei Unterschreitung der Stoppabstands sofort `ERROR;OBSTACLE` aus
- Richtungsabhängig: bei X-Bewegung relevant, bei Z ggf. anders (noch festzulegen je nach Montageposition)

### Türsensor (VL53L0X, aktuell verbaut)

- Sitzt fest am Schlitten
- Wird **nur ausgewertet, wenn der Schlitten die Zielposition erreicht hat** (nicht während der Fahrt)
- Zweck: Prüfen ob die Druckertür wirklich geöffnet ist
- Schwellwert: Distanz > ~200 mm = Tür offen (genauen Wert noch festlegen)
- Ergebnis landet im STATUS-Feld `door_dist_mm`; die Entscheidung „Tür offen ja/nein" trifft der Pi

---

## Zustandscodes

| Code | Bedeutung |
|---|---|
| `NOT_REFERENCED` | Bereit, aber noch keine Referenzfahrt |
| `READY` | Referenziert, wartet auf Kommando |
| `BUSY_HOMING` | Referenzfahrt läuft |
| `BUSY_MOVING` | Fahrt zu Zielposition läuft |
| `STOPPED` | Bewegung per STOP angehalten |
| `ERROR` | Fehler, alle Motoren gestoppt, wartet auf RESET_ERROR |

---

## Fehlercodes

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

## Zustandsübergänge

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

## Kommunikationsablauf: typische Sequenzen

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
← EVT;0;STATUS;state=READY;...;door_dist_mm=312
  (Pi wertet door_dist_mm aus: 312 > 200 → Tür offen)
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
← EVT;0;HEARTBEAT;uptime_ms=45231;state=READY;x=350;z=120
← EVT;0;HEARTBEAT;uptime_ms=46231;state=READY;x=350;z=120
```

---

## Offene Punkte

| Thema | Frage |
|---|---|
| Heartbeat-Intervall | Wie oft soll der ESP den Heartbeat senden? |
| Türsensor-Schwelle | Welche Distanz gilt als „Tür offen"? (~200 mm, noch zu bestätigen) |
| Hindernissensor-Typ | Welcher Sensor, welche Reichweite, welche Montageposition am Schlitten? |
| Servo-Steuerung | Direkt per SET_CLAMP vom Pi, oder implizit durch ESP-interne Sequenz? |
| Türarm-Aktor | Stepper oder Servo? (beeinflusst interne ESP-Implementierung, nicht das Protokoll) |
| Achsreihenfolge bei MOVE_TO | Fährt der ESP immer beide Achsen gleichzeitig, oder erst Z dann X (o.ä.)? |
| Toleranz Zielposition | Welche Abweichung in mm ist beim MOVE_DONE noch akzeptabel? |
| Positionsformat | Reichen ganzzahlige mm, oder wird 0,1 mm Auflösung benötigt? |

---

## Hardwarebelegung ESP32

| Funktion | Pin | Hinweis |
|---|---|---|
| UART zum Pi | USB / UART0 | 115200 Baud |
| I2C SDA (Türsensor) | GPIO 21 | 100 kHz |
| I2C SCL (Türsensor) | GPIO 22 | 100 kHz |
| Türsensor XSHUT | GPIO 16 | VL53L0X |
| Hindernissensor | noch offen | Typ noch nicht festgelegt |
| Stepper X – STEP | noch offen | |
| Stepper X – DIR | noch offen | |
| Stepper X – ENABLE | noch offen | active-low |
| Stepper Z – STEP | noch offen | |
| Stepper Z – DIR | noch offen | |
| Stepper Z – ENABLE | noch offen | active-low |
| Servo PWM | noch offen | LEDC, 50 Hz |
| Greifer-Taster | noch offen | active-low |
| Home-Taster X | noch offen | active-low |
| Home-Taster Z | noch offen | active-low |
| Türarm-Aktor | noch offen | Stepper oder Servo, noch nicht entschieden |

---

## Timing und Parameter

| Parameter | Wert | Hinweis |
|---|---|---|
| Baud Rate | 115200 | |
| Heartbeat-Intervall | noch offen | ESP → Pi |
| Stream-Intervall | noch festzulegen | bei STREAM_ON |
| Hindernissensor Stoppabstand | noch offen | |
| Hindernissensor Warnabstand | noch offen | |
| Türsensor Schwelle „offen" | ~200 mm | noch zu bestätigen |
| Homing-Timeout | noch festzulegen | |
| Bewegungs-Timeout | noch festzulegen | |
| Positionstoleranz | noch festzulegen | in mm |

# ESP-Schlitten – Vollsystem

Projekt G6-TWIE23A · DHBW Stuttgart · IoT 2026

ESP32-Firmware für einen 4-Achs-Schlitten zur automatisierten Druckplatten-Entnahme.
Der ESP kommuniziert über UART/USB mit einem Raspberry Pi, der die übergeordnete Steuerung übernimmt.

---

## Hardware

| Komponente | Treiber | Funktion |
|---|---|---|
| Schrittmotor X | CL42T (Closed-Loop) | Schlitten horizontal |
| Schrittmotor Z | CL42T (Closed-Loop) | Schlitten vertikal |
| Schrittmotor Greifer | DRV8825 | Greifer ein-/ausfahren |
| Schrittmotor Türarm | DRV8825 | Türarm ein-/ausfahren |
| VL53L0X | I2C 0x29 | Türsensor (misst ob Druckertür offen ist) |
| TF-Luna LiDAR | I2C 0x10 | Hindernissensor in Fahrtrichtung |
| Greifer-Endschalter | GPIO 36 | Referenzposition Greifer |
| Türarm-Endschalter | GPIO 39 | Referenzposition Türarm |
| Platten-Taster | GPIO 34 | Erkennt aufgelegte Druckplatte |

Schlitten-Endschalter (X, Z) sind am Raspberry Pi angeschlossen – der Pi leitet
das Signal per `HOME_SWITCH_HIT`-Kommando an den ESP weiter.

Pinbelegung vollständig in [`src/config/Pins.h`](../../src/config/Pins.h).

---

## Kommunikation Pi → ESP

**Verbindung:** UART over USB, 115200 Baud, 8N1, ASCII, eine Nachricht pro Zeile (`\n`)  
**Feldtrenner:** Semikolon `;`  
**Positionsangaben:** immer Millimeter (Ganzzahl)

Jedes Kommando hat die Form:

```
CMD;<id>;<befehl>[;<key>=<value>...]
```

`<id>` ist eine vom Pi vergebene Ganzzahl. Der ESP spiegelt sie in allen Antworten zurück.

| Kommando | Syntax | Voraussetzung |
|---|---|---|
| PING | `CMD;<id>;PING` | immer |
| STATUS | `CMD;<id>;STATUS` | immer |
| STOP | `CMD;<id>;STOP` | immer außer ERROR |
| STREAM ON/OFF | `CMD;<id>;STREAM_ON` / `STREAM_OFF` | immer |
| HOME | `CMD;<id>;HOME` | nicht busy, nicht ERROR |
| HOME_SWITCH_HIT | `CMD;<id>;HOME_SWITCH_HIT;axis=<X\|Z>` | nur in BUSY_HOMING oder BUSY_MOVE_HOME |
| MOVE_TO | `CMD;<id>;MOVE_TO;x=<mm>;z=<mm>` | READY oder STOPPED, referenziert |
| MOVE_HOME | `CMD;<id>;MOVE_HOME` | READY oder STOPPED, referenziert |
| RESET_ERROR | `CMD;<id>;RESET_ERROR` | nur in ERROR |
| PICKUP | `CMD;<id>;PICKUP;gripper_depth=<mm>;lift_offset=<mm>` | READY oder STOPPED, referenziert |
| DEPOSIT | `CMD;<id>;DEPOSIT;gripper_depth=<mm>;lift_offset=<mm>` | READY oder STOPPED, referenziert |
| OPEN_DOOR | `CMD;<id>;OPEN_DOOR;x_approach=<mm>;z_approach=<mm>;arm_extend=<mm>;radius=<mm>;angle=<deg>;hook_drop=<mm>` | READY oder STOPPED, referenziert |
| CLOSE_DOOR | `CMD;<id>;CLOSE_DOOR;x_approach=<mm>;z_approach=<mm>;arm_extend=<mm>;radius=<mm>;angle=<deg>;hook_drop=<mm>` | READY oder STOPPED, referenziert |

---

## Kommunikation ESP → Pi

### Sofortantwort auf ein Kommando (RSP)

```
RSP;<id>;ACK          → Kommando akzeptiert, Aktion läuft
RSP;<id>;ERR;<code>   → Kommando abgelehnt, kein Motor gestartet
```

### Ereignisse (EVT)

```
EVT;<id>;OK;<event>    ;x=<mm>;z=<mm>
EVT;0;STATE;<zustand>  ;ref=<0|1>;x=<mm>;z=<mm>
EVT;0;STATUS;state=<z> ;error=<e>;ref=<0|1>;x=<mm>;z=<mm>;target_x=<mm>;target_z=<mm>;busy=<0|1>;gripper_home=<0|1>;door_arm_home=<0|1>;obstacle_ok=<0|1>;door_open=<0|1>;door_dist_mm=<mm>;plate_detected=<0|1>
EVT;0;ERR;<code>       ;x=<mm>;z=<mm>
EVT;0;HEARTBEAT;uptime_ms=<ms>;state=<z>;x=<mm>;z=<mm>
```

**EVT OK** – Abschluss einer Aktion (id = auslösendes Kommando):

| event | Auslöser |
|---|---|
| `PONG` | Antwort auf PING |
| `HOME_DONE` | Referenzfahrt abgeschlossen |
| `MOVE_DONE` | Zielposition erreicht |
| `MOVE_HOME_DONE` | Rückfahrt zur Home-Position abgeschlossen |
| `STOPPED` | STOP ausgeführt |
| `ERROR_RESET` | RESET_ERROR ausgeführt |
| `DOOR_OPEN_DONE` | Tür geöffnet, Schlitten zurück auf Ausgangsposition |
| `DOOR_CLOSE_DONE` | Tür geschlossen, Arm eingefahren |
| `PICKUP_DONE` | Plattenentnahme abgeschlossen |
| `DEPOSIT_DONE` | Plattenablage abgeschlossen |

**EVT STATE** – spontan bei jedem Zustandswechsel, id immer `0`.  
**EVT STATUS** – auf STATUS-Kommando, bei jedem Eintritt in ERROR, und periodisch wenn Stream aktiv.  
**EVT HEARTBEAT** – alle 1000 ms.

---

## Zustandsmaschine

```
boot → NOT_REFERENCED
         │ HOME
         ▼
    BUSY_HOMING ──[Fehler]──────────────────────────────► ERROR
         │ HOME_DONE                                        ▲   │ RESET_ERROR
         ▼                                                  │   ▼
       READY ◄──────────────────────────────────────────────  NOT_REFERENCED
         │
         ├─ MOVE_TO (aus x=0,z=0) ──► BUSY_SCANNING ──[ok]──► BUSY_MOVING ──► READY
         ├─ MOVE_TO (sonst) ──────────────────────────────────► BUSY_MOVING ──► READY
         ├─ MOVE_HOME ────────────────────────────────────────► BUSY_MOVE_HOME ► READY
         ├─ PICKUP ───────────────────────────────────────────► BUSY_PICKUP ──► READY
         ├─ DEPOSIT ──────────────────────────────────────────► BUSY_DEPOSIT ──► READY
         ├─ OPEN_DOOR ────────────────────────────────────────► BUSY_OPEN_DOOR ► READY
         └─ CLOSE_DOOR ───────────────────────────────────────► BUSY_CLOSE_DOOR ► READY
```

`STOP` ist aus jedem Zustand außer ERROR möglich → landet in `STOPPED`.  
`STOPPED` verhält sich wie `READY`.  
Jeder Fehler in einem `BUSY_*`-Zustand → sofort `ERROR`, alle Motoren gestoppt.

### Zustandscodes

| Code | Bedeutung |
|---|---|
| `NOT_REFERENCED` | Bereit, aber noch keine Referenzfahrt |
| `READY` | Referenziert, wartet auf Kommando |
| `BUSY_HOMING` | Referenzfahrt läuft |
| `BUSY_SCANNING` | Z-Scan vor Fahrt aus Home-Position |
| `BUSY_MOVING` | Fahrt zu Zielposition |
| `BUSY_MOVE_HOME` | Rückfahrt zur Home-Position |
| `BUSY_PICKUP` | Plattenentnahme läuft |
| `BUSY_DEPOSIT` | Plattenablage läuft |
| `BUSY_OPEN_DOOR` | Türöffnung läuft |
| `BUSY_CLOSE_DOOR` | Türschließung läuft |
| `STOPPED` | Per STOP angehalten |
| `ERROR` | Fehler, wartet auf RESET_ERROR |

### Fehlercodes

| Code | Ursache |
|---|---|
| `INVALID_COMMAND` | Unbekanntes oder fehlerhaftes Kommando |
| `BUSY` | ESP gerade beschäftigt |
| `NOT_REFERENCED` | MOVE_TO ohne vorherige Referenzfahrt |
| `INVALID_STATE` | Kommando im falschen Zustand |
| `OBSTACLE` | Hindernissensor unterschreitet Stoppabstand |
| `MOVE_TIMEOUT` | Zielposition nicht rechtzeitig erreicht |
| `HOMING_TIMEOUT` | Referenzfahrt nicht rechtzeitig abgeschlossen |
| `DRIVER_FAULT` | CL42T meldet Alarm (ALM-Pin) |
| `PLATE_NOT_DETECTED` | Plattenerkennungs-Taster hat nach PICKUP nicht ausgelöst |
| `DOOR_NOT_OPEN` | Türsensor meldet zu geringe Distanz vor PICKUP / DEPOSIT |

---

## Typische Abläufe

### Referenzfahrt

Die Schlitten-Endschalter (X, Z) sind am Pi angeschlossen. Der Pi muss bei ausgelöstem
Taster sofort `HOME_SWITCH_HIT` schicken.

```
→ CMD;1;HOME
← RSP;1;ACK
← EVT;0;STATE;BUSY_HOMING;ref=0;x=0;z=0
  [ESP fährt X Richtung Endschalter, Greifer und Türarm gleichzeitig]
  [Pi erkennt X-Endschalter]
→ CMD;2;HOME_SWITCH_HIT;axis=X
← RSP;2;ACK
  [ESP stoppt X, setzt X=0, fährt Z Richtung Endschalter]
  [Pi erkennt Z-Endschalter]
→ CMD;3;HOME_SWITCH_HIT;axis=Z
← RSP;3;ACK
  [ESP wartet auf Greifer- und Türarm-Endschalter (intern)]
← EVT;1;OK;HOME_DONE;x=0;z=0
← EVT;0;STATE;READY;ref=1;x=0;z=0
```

### Fahrt zu Zielposition (aus Home – mit Z-Scan)

Wenn der Schlitten an x=0, z=0 steht, fährt der ESP die Z-Achse zuerst komplett hoch
und prüft dabei den Hindernissensor.

```
→ CMD;2;MOVE_TO;x=350;z=120
← RSP;2;ACK
← EVT;0;STATE;BUSY_SCANNING;ref=1;x=0;z=0
← EVT;0;STATE;BUSY_MOVING;ref=1;x=0;z=240
← EVT;2;OK;MOVE_DONE;x=350;z=120
← EVT;0;STATE;READY;ref=1;x=350;z=120
```

### Plattenentnahme (PICKUP)

Schlitten steht bereits auf der Zielposition vor dem Drucker. Tür muss offen sein.

```
→ CMD;5;PICKUP;gripper_depth=120;lift_offset=8
← RSP;5;ACK
← EVT;0;STATE;BUSY_PICKUP;ref=1;x=350;z=120
  [Greifer fährt 120 mm aus]
  [Z-Achse hebt 8 mm → Platte liegt auf Gabel]
  [Greifer fährt ein]
← EVT;5;OK;PICKUP_DONE;x=350;z=128
← EVT;0;STATE;READY;ref=1;x=350;z=128
```

### Plattenablage (DEPOSIT)

Schlitten steht auf Zielposition, trägt eine Platte.

```
→ CMD;6;DEPOSIT;gripper_depth=120;lift_offset=8
← RSP;6;ACK
← EVT;0;STATE;BUSY_DEPOSIT;ref=1;x=500;z=128
  [Z-Achse hebt 8 mm → Platte über Stellfläche]
  [Greifer fährt 120 mm aus]
  [Z-Achse senkt 8 mm → Platte liegt auf]
  [Greifer fährt ein]
← EVT;6;OK;DEPOSIT_DONE;x=500;z=120
← EVT;0;STATE;READY;ref=1;x=500;z=120
```

### Tür öffnen (OPEN_DOOR)

Der Türarm führt einen Kreisbogen aus. Geometrie: `arm(θ) = arm_extend + radius·sin(θ)`,
`Δx(θ) = radius·(cos(θ)−1)`.

```
→ CMD;7;OPEN_DOOR;x_approach=370;z_approach=105;arm_extend=30;radius=150;angle=160;hook_drop=15
← RSP;7;ACK
← EVT;0;STATE;BUSY_OPEN_DOOR;ref=1;x=350;z=120
  [Schlitten fährt Anfahrposition an: X=370, Z=105]
  [Türarm fährt 30 mm aus]
  [Z-Achse hebt 15 mm → eingehakt]
  [Kreisbogen öffnen: Arm konstant, X folgt]
  [Z-Achse senkt 15 mm → ausgehakt]
  [Türarm fährt ein, Schlitten zurück zur Ausgangsposition]
← EVT;7;OK;DOOR_OPEN_DONE;x=350;z=120
← EVT;0;STATE;READY;ref=1;x=350;z=120
```

### Tür schließen (CLOSE_DOOR)

Anfahrposition = Ende des OPEN_DOOR-Bogens:
`x_close = x_open_approach + radius·(cos(angle)−1)`

```
→ CMD;10;CLOSE_DOOR;x_approach=79;z_approach=105;arm_extend=30;radius=150;angle=160;hook_drop=15
← RSP;10;ACK
← EVT;0;STATE;BUSY_CLOSE_DOOR;ref=1;x=350;z=120
  [Schlitten fährt X=79, Z=105]
  [Türarm fährt auf Grifftiefe: 30 + 150·sin(160°) ≈ 81 mm]
  [Z-Achse hebt 15 mm → eingehakt]
  [Kreisbogen schließen: Arm rückwärts, X folgt]
  [Z-Achse senkt 15 mm → ausgehakt]
  [Türarm fährt ein, Schlitten zurück zur Ausgangsposition]
← EVT;10;OK;DOOR_CLOSE_DONE;x=350;z=120
← EVT;0;STATE;READY;ref=1;x=350;z=120
```

### Fehlerfall mit Recovery

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

---

## Detaillierte Protokoll-Referenz

→ [`Doku/Schnittstellen.md`](../../Doku/Schnittstellen.md)

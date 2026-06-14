# Jeep Embedded Controls

A custom embedded security and control system for a 1995 Jeep Wrangler, built as a
personal project to demonstrate full-stack embedded systems development across firmware,
hardware, PCB design, power electronics, and mobile applications.

## Project Overview

This system provides remote control, alarm/immobilization, and real-time GPS tracking
for a 1995 Jeep Wrangler using two custom-designed PCBs, 3D printed enclosures, and a
companion web app. A keyfob and a main controller communicate wirelessly via ESP-NOW,
with 4G LTE cellular for remote GPS tracking accessible from any device.

The system is designed around a **battery-backed, low-power security architecture**: the
controller runs from its own auxiliary battery so it keeps tracking and alarming even if
the vehicle's main battery is disconnected — the exact scenario a theft involves — while
never drawing the vehicle battery down when parked.

## Demo

> Live GPS tracker web app: [Jeep Tracker](https://matt99med.github.io/Jeep-Embedded-Controls/)

## Features

- Remote kill switch (immobilizer) via custom ESP-NOW keyfob
- **Hidden manual bypass switch** — fail-safe so a controller fault never strands the vehicle
- Hood and shock sensor triggered siren with automatic arming
- Remote arm/disarm via keyfob with LED status feedback
- Real-time GPS tracking via 4G LTE cellular (SIM7600G-H)
- Web app for live location tracking via MQTT over HiveMQ Cloud
- **Auxiliary backup battery** — system survives main-battery disconnect
- **Vehicle-sensing charge control** — aux battery charges only while the alternator runs,
  so the vehicle battery is never drained while parked (months of standby)
- **Deep-sleep architecture** — wakes on hood/shock/timer events; tens of µA when parked
- Hardware low-voltage disconnect protecting the aux battery from over-discharge
- LiPo battery powered keyfob with USB-C charging

## Hardware

### Main Controller

- **ESP32-WROOM-32E** module (240MHz dual-core, Wi-Fi, BT)
- **CP2102N** USB-to-UART bridge with auto-reset programming circuit (USB-C)
- **SIM7600G-H** 4G LTE + GPS (Waveshare breakout, socketed)
- **Auxiliary battery system:** 12V sealed AGM battery
- **Vehicle-sensing charge relay:** connects vehicle 12V to aux battery only when the
  alternator is running (firmware-controlled, AO3400A driver, off-board relay)
- **Power rails:**
  - LM5164-Q1 wide-Vin synchronous buck (battery -> 3.3V, ~10uA quiescent)
  - MP2307DN buck (battery -> 3.8V for SIM7600, gated by GPIO for sleep)
  - Hardware UVLO low-voltage disconnect on the 3.3V buck
- Relay/fuse box for kill switch + siren (off-board high current)
- BAT54S input clamps + RC filtering on hood/shock sensor inputs
- SMCJ33A TVS protection on 12V inputs
- 4 firmware-controlled status LEDs (system / cellular / armed / fault)
- Custom 2-layer KiCad PCB, 154.5 x 120 mm
- Custom Fusion 360 enclosure (in progress)

### Keyfob

- **ESP32-C3FH4X** (bare QFN32, 160MHz RISC-V, in-package 4MB flash)
- **Johanson 2450AT18A100E-AEC** chip antenna with L-C-L matching network
- 40MHz crystal (NX3225GD-40M)
- MCP73831 LiPo charger IC
- 3.7V LiPo battery
- USB-C charging + native USB programming (GPIO18/19)
- AP2112K-3.3 LDO
- Tactile buttons: Lock, Unlock + BOOT/RESET
- Status LEDs: charge + connection
- Slide power switch
- Custom 2-layer KiCad PCB, 30 x 50 mm
- Custom Fusion 360 enclosure (in progress)

## Tech Stack

| Category        | Technology                            |
| --------------- | ------------------------------------- |
| Firmware        | ESP-IDF v6.0 (C)                      |
| RTOS            | FreeRTOS                              |
| Wireless        | ESP-NOW (keyfob <-> controller)       |
| Cellular        | SIM7600G-H AT commands over UART2     |
| IoT Protocol    | MQTT over TLS (HiveMQ Cloud)          |
| PCB Design      | KiCad 10.0                            |
| 3D Modeling     | Autodesk Fusion 360                   |
| Web App         | HTML/CSS/JavaScript + Google Maps API |
| Version Control | Git/GitHub                            |
| PCB Fabrication | JLCPCB (assembled)                    |

## System Architecture

```
[Keyfob ESP32-C3] <--ESP-NOW 2.4GHz--> [Main ESP32 Controller]
[Lock/Unlock]                            [Kill Switch / Siren]
                                         [Hood + Shock Sensors]
                                         [Aux Battery + Charge Control]
                                         [Hidden Bypass Switch]
                                                 |
                                         [SIM7600G-H Module]
                                                 |
                                      [Hologram IoT SIM / 4G LTE]
                                                 |
                                        [HiveMQ MQTT Broker]
                                                 |
                                     [Web App / iPhone / Any Device]
```

## Power Architecture

```
[Vehicle 12V] --fused--> [Charge Relay] --(only when alternator running)--> [Aux 12V Battery]
                              ^                                                    |
                       [GPIO sense/control]                                        |
                                                                                   v
                                                          +------------------------+
                                                          |                        |
                                                  [LM5164 Buck]            [MP2307 Buck]
                                                   3.3V (always-on,         3.8V (gated,
                                                   UVLO protected)          SIM7600)
                                                          |                        |
                                                  [ESP32 + logic]          [SIM7600G-H]
```

Key property: when parked, the vehicle battery and the aux battery are physically
disconnected. The controller draws only from the aux battery, so the vehicle always
starts no matter how long it sits.

## Firmware Architecture

The main controller firmware is built around a **tiered low-power state machine** rather
than always-on tasks, to achieve months of parked standby on the auxiliary battery.

### Operating Model

- **Deep sleep** is the default parked state — modem off, CPU halted, only RTC wake
  sources armed. Draw is in the tens of uA.
- **Wake sources:** hood sensor (RTC GPIO), shock sensor (RTC GPIO), or a wake timer for
  scheduled MQTT check-ins. The check-in interval shortens automatically when armed.
- **Active window:** after any activity the controller stays in *light sleep* so the
  radio remains listenable, giving near-instant ESP-NOW lock/unlock response. After an
  idle timeout it drops back to deep sleep.
- On every wake the controller reads vehicle + battery voltage, runs charge control, and
  — if armed and triggered — fires the siren, immobilizes, and pushes an alert + GPS
  burst over cellular.

### Kill Relay Logic (fail-safe)

The kill relay is **energized-to-run**: driving the coil HIGH closes the starter circuit
(vehicle can start); LOW immobilizes. A dead controller, dead battery, or cut wire
therefore fails *immobilized*, and the hidden mechanical bypass switch is the recovery
path. The bypass is intentionally not wired to the PCB.

### Charge Control

Firmware closes the charge relay above 13.2 V on the vehicle-sense line (alternator
running) and opens it below 12.8 V, with hysteresis to avoid chatter. The relay coil is
fed from the vehicle side, so charging is physically impossible when the engine is off
regardless of firmware state.

### Source Layout

```
main_controller/main/
├── main.c            # wake-reason dispatch, tiered sleep state machine
├── config.h          # pin map, thresholds, ESP-NOW protocol
├── system_state.h    # shared state + arm/alarm/charge API
├── gpio_handler.c    # ADC sensing, relay control, arm/alarm, charge logic
├── espnow_handler.c  # keyfob command receiver
└── sim7600.c         # modem power sequencing, GPS, MQTT

keyfob/main/
├── main.c            # deep-sleep wake-on-button, send, LED ack, sleep
├── config.h          # pin map + ESP-NOW protocol
├── espnow_keyfob.c   # one-shot ESP-NOW transmit
└── espnow_keyfob.h
```

### ESP-NOW Message Protocol

| Command     | Value | Action                             |
| ----------- | ----- | ---------------------------------- |
| CMD_LOCK    | 0x01  | Arm siren + immobilize (kill LOW)  |
| CMD_UNLOCK  | 0x02  | Disarm + allow run (kill HIGH)     |

### MQTT Topics

| Topic            | Direction   | Payload                     |
| ---------------- | ----------- | --------------------------- |
| jeep/gps/request | App -> ESP32 | "request"                  |
| jeep/gps/data    | ESP32 -> App | {"lat":xx.xxx,"lon":xx.xxx} |
| jeep/status      | ESP32 -> App | "armed" or "disarmed"       |
| jeep/alert       | ESP32 -> App | power-cut / tamper event    |

> Broker credentials and APN are provided via NVS / `idf.py menuconfig`, not committed to
> source.

## PCB Design

### Keyfob PCB

- 2-layer board, 30 x 50 mm
- All 0402 passives, JLCPCB assembled
- Bare ESP32-C3FH4X with Johanson chip antenna + matching network
- GND copper pour on B.Cu with via stitching
- M2 mounting holes
- USB-C edge connector for charging + programming
- **Status: ordered from JLCPCB (assembled) — awaiting delivery**

### Main Controller PCB

- 2-layer board, 154.5 x 120 mm
- Automotive environment rated design
- Bare ESP32-WROOM-32E + CP2102N programming
- Dual switching power supplies (LM5164 + MP2307)
- Aux battery charge management + UVLO
- Screw-terminal field wiring, socketed SIM7600 carrier
- GND copper pour on B.Cu with via stitching
- M3 mounting holes in all 4 corners
- **Status: ordered from JLCPCB (assembled) — awaiting delivery**

## Status

- [x] Hardware redesign complete (KiCad schematics — both boards, bare-chip)
- [x] Battery-backed power architecture with vehicle-sensing charge control
- [x] Firmware rework for deep-sleep / charge-control architecture
- [x] Keyfob PCB ordered (JLCPCB assembled)
- [x] Main controller PCB ordered (JLCPCB assembled)
- [ ] PCB bring-up and board-level testing
- [ ] SIM7600 MQTT/cellular integration (broker connect + report cycle)
- [ ] 3D printed enclosures (Fusion 360 — in progress)
- [ ] Hidden bypass switch installation
- [ ] System integration and testing
- [ ] Final installation in 1995 Jeep Wrangler

## Author

**Matthew Medina**

California State University Fullerton — BS/MS Computer Engineering

GitHub: [matt99med](https://github.com/matt99med)
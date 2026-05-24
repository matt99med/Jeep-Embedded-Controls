# Jeep Embedded Controls

A custom embedded security and control system for a 1995 Jeep Wrangler, built as a 
personal project to demonstrate full-stack embedded systems development.

## Project Overview

This system provides remote control and real-time GPS tracking for a 1995 Jeep Wrangler
using two custom-designed PCBs, 3D printed enclosures, and a companion iOS app.

## Features

- Remote kill switch via custom ESP-NOW keyfob
- Hood sensor triggered siren system
- Remote arm/disarm via keyfob
- Real-time GPS tracking via 4G LTE cellular (SIM7600G-H)
- iOS app for live location tracking
- Control of auxiliary off-road lights (lightbar, spot, fog, pod, chase)
- Panic button on keyfob for immediate siren activation

## Hardware

### Main Controller (Engine Bay)
- ESP32-DevKit V1
- SIM7600G-H 4G LTE + GPS module
- ULN2803 Darlington output array
- SP721ABG input protection
- LM25576 buck converter (12V → 5V)
- LM74810 automotive ideal diode
- Custom KiCad PCB design
- Custom Fusion 360 enclosure

### Keyfob
- ESP32-C3 Super Mini
- MCP73831 LiPo charger
- 3.7V 110mAh LiPo battery
- USB-C charging
- 3 tactile buttons (Lock, Unlock, Panic)
- 3 LED indicators (Red, Green, Blue)
- Custom KiCad PCB design
- Custom Fusion 360 enclosure

## Tech Stack

| Category | Technology |
|---|---|
| Firmware | ESP-IDF (C) |
| Wireless | ESP-NOW |
| Cellular | SIM7600G-H AT commands |
| PCB Design | KiCad |
| 3D Modeling | Autodesk Fusion 360 |
| Mobile App | Swift (iOS) |
| Version Control | Git/GitHub |

## Repository Structure

    Jeep-Embedded-Controls/
    ├── main_controller/     # ESP32 firmware for main jeep controller
    ├── keyfob/             # ESP32-C3 firmware for keyfob
    ├── schematics/         # KiCad schematic exports
    ├── pcb/                # PCB layout files
    ├── fusion360/          # 3D enclosure design exports
    └── README.md

## System Architecture

    [Keyfob ESP32-C3] <--ESP-NOW--> [Main ESP32]
                                          |
                                  [SIM7600G-H]
                                          |
                                  [Hologram IoT]
                                          |
                                  [iOS App]

## Status

- [x] Hardware design complete (KiCad schematics)
- [x] Main controller firmware (ESP-IDF / FreeRTOS)
- [x] Keyfob firmware (ESP-IDF / ESP-NOW)
- [x] Web app (MQTT + Google Maps)
- [ ] PCB layout (KiCad)
- [ ] PCB fabrication (JLCPCB)
- [ ] 3D printed enclosures (Fusion 360)
- [ ] System integration and testing

## Author

**Matthew Medina**  

California State University Fullerton — BS/MS Computer Engineering  

GitHub: [matt99med](https://github.com/matt99med)
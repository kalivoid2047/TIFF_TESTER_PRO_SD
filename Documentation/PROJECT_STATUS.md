# TIFF_TESTER_PRO_SD — Project Status

_Last updated: 2026-08-18_

## Overview

TIFF_TESTER_PRO_SD is a bench-first automotive module tester / diagnostic / service
platform built around a dual-controller architecture: an ESP32 application
controller and an independent Arduino Nano safety controller. The design goal
is that the Nano can always cut power to the device under test (DUT),
regardless of what the ESP32 is doing.

## Repository layout

```
TIFF_TESTER_PRO_SD/
├── Arduino_Nano_Safety/
│   └── TIFF_TESTER_PRO_SD_NANO.ino      Nano safety controller firmware
├── ESP32_Firmware/
│   └── TIFF_TESTER_PRO_SD_ESP32.ino     ESP32 main application firmware
├── SD_MODULES/
│   └── MODULES/TOYOTA/HILUX_1KD_TURBO.INI  Example module profile
└── Documentation/
    ├── README.md                        Project overview
    └── PINOUT_AND_WIRING.txt            Inter-controller & sensor wiring
```

No build system, package manifest, or test suite is present. Both firmware
files are standalone Arduino `.ino` sketches (ESP32 board package / AVR core),
intended to be compiled/uploaded individually via the Arduino IDE or
arduino-cli. There is no git repository initialized yet.

## Architecture

### ESP32 (`ESP32_Firmware/TIFF_TESTER_PRO_SD_ESP32.ino`)
- Boots a Wi-Fi Access Point (`TIFF_TESTER` / password `tifftester`) and serves
  a small built-in web UI (`WebServer` on port 80, AP IP `192.168.4.1`).
- Talks to the Nano over `HardwareSerial(2)` (GPIO16/17) using a simple
  line-based CSV protocol.
- Sends `HEARTBEAT` every 250 ms and polls `STATUS` every 500 ms; parses the
  Nano's `STATUS,relay,fault,estop,watchdog,supply,dut,current,faulttext` line.
- Reads its own DUT voltage/current and supply-voltage ADC pins (GPIO34/35/32)
  independently (currently unused for logic beyond raw reads).
- SD card (SPI, CS=GPIO5) holds `/MODULES`, `/REPORTS`, `/LOGS`. Module
  profiles are `.INI` text files.
- Web API:
  - `GET /` — status page + module upload form
  - `GET /api/status` — plain-text Nano status
  - `GET /api/power/on` / `GET /api/power/off` — relay request (forwarded to Nano)
  - `POST /api/module` — validates and saves a pasted `.INI` module profile
  - `GET /api/modules` — lists files on the SD card
- Module validation (`validModuleText`) is minimal: checks for `[MODULE]`,
  `id=`, `max_current_a=`, `min_voltage_v=`, `max_voltage_v=` and a minimum
  length; does not otherwise parse or type-check the file.
- CAN (MCP2515, CS=GPIO15/INT=GPIO4) and K-Line (L9637D) hardware are called
  out in the file header/wiring doc as intended peripherals, but no CAN/K-Line
  driver code, diagnostic engine, or UDS logic exists in this file yet — the
  header comment explicitly warns not to send arbitrary UDS routines.

### Arduino Nano (`Arduino_Nano_Safety/TIFF_TESTER_PRO_SD_NANO.ino`)
- Independent fail-safe controller; relay is OFF at boot and OFF whenever
  anything looks wrong.
- Accepts UART commands from the ESP32: `HEARTBEAT`, `POWER_ON`, `POWER_OFF`,
  `STATUS`, `RESET_FAULT`.
- Continuously supervises, regardless of commands received:
  - Supply voltage in range `11.0–15.0 V`
  - DUT voltage ≤ `15.0 V`
  - Current ≤ `5.0 A`
  - Emergency-stop input (D3, `INPUT_PULLUP`, active LOW)
  - ESP32 heartbeat freshness (2000 ms timeout)
- Uses AVR hardware watchdog (`wdt_enable(WDTO_2S)`, reset each loop) so a
  Nano firmware lockup itself triggers a hardware reset (relay defaults LOW on
  boot).
- ADC scaling constants (`SUPPLY_DIVIDER`, `DUT_DIVIDER`, `CURRENT_ZERO_V`,
  `CURRENT_V_PER_A`) are explicitly marked as placeholders — the header
  comment states they must be calibrated against the real voltage-divider and
  current-sensor hardware before connecting a DUT.
- Relay logic assumes active-high relay control (`HIGH` = energize); a code
  comment flags that this must be inverted if an active-low relay module is
  used.

### Module profile format (`SD_MODULES/MODULES/TOYOTA/HILUX_1KD_TURBO.INI`)
One example module exists: a Toyota Hilux 1KD-FTV turbo actuator profile.
Sections present: `[MODULE]` (id/manufacturer/vehicle/engine metadata),
`[SAFETY]` (voltage/current limits), `[TEST_RESISTANCE]`, `[TEST_POSITION]`,
`[COMMUNICATION]` (protocol=CAN, bitrate=500000), `[TESTS]` (flags for which
test routines apply), `[SERVICE]` (OEM calibration/learn routines, currently
all disabled with `=0`). This file defines the schema other module profiles
are expected to follow, but nothing in the firmware yet executes the
`[TESTS]`/`[SERVICE]` routines described in it — the ESP32 only validates and
stores the raw text.

## Wiring (see `Documentation/PINOUT_AND_WIRING.txt`)

| Signal | ESP32 | Nano |
|---|---|---|
| Inter-MCU UART | GPIO16 (RX2) / GPIO17 (TX2) | D0/D1 |
| SD card CS | GPIO5 | — |
| CAN CS / INT | GPIO15 / GPIO4 | — |
| DUT voltage ADC | GPIO34 | A0 |
| DUT current ADC | GPIO35 | A2 |
| Supply voltage ADC | GPIO32 | A1 |
| DUT relay driver | — | D4 |
| Emergency stop | — | D3 (INPUT_PULLUP) |
| Buzzer | — | D6 |
| Status LED | — | D13 |
| Temp/spare | — | A3 |

Explicit hardware warnings: relay logic polarity must match the physical
relay module; automotive 12 V signals must never touch ESP32/Nano GPIOs
directly — dividers/protection/isolation are required.

## What's implemented

- [x] Dual-MCU fail-safe architecture with independent Nano supervision
- [x] Heartbeat-based dead-man's switch (ESP32 → Nano)
- [x] Emergency stop, over/under-voltage, over-current cutoffs
- [x] Nano hardware watchdog
- [x] ESP32 Wi-Fi AP + minimal web UI
- [x] SD-based module storage with basic upload validation
- [x] One example module profile establishing the `.INI` schema

## What's not implemented / explicitly flagged as incomplete

- [ ] CAN bus driver/stack (MCP2515) — pins defined, no driver code
- [ ] K-Line interface (L9637D) — mentioned in header, no code
- [ ] Any UDS/OEM diagnostic or service routine execution — `[TESTS]` and
      `[SERVICE]` sections in module profiles are not consumed by firmware
- [ ] Test report generation/saving to `/REPORTS`, and `/LOGS` usage (dirs are
      created but nothing writes to them yet)
- [ ] ADC calibration — divider ratios and current-sensor zero/scale on the
      Nano are placeholder values, explicitly marked "replace before use"
- [ ] Relay polarity confirmation against actual relay hardware
- [ ] Android/Bluetooth interface (mentioned in README as "future")
- [ ] Wi-Fi AP password is a hardcoded default (`tifftester`) — fine for bench
      use, not reviewed for anything beyond that
- [ ] No automated tests, no CI, no version control initialized in this folder

## Suggested next steps

1. Calibrate and hard-code real voltage-divider ratios and current-sensor
   constants on the Nano against bench multimeter readings.
2. Confirm/adjust relay output polarity for the physical relay module in use.
3. Decide on and implement report writing to `/REPORTS` (format, filename
   scheme) so bench sessions produce a saved artifact.
4. Add MCP2515 CAN driver integration and a minimal CAN request/response path
   before attempting any module communication test.
5. Define how `[TESTS]` entries in a module `.INI` map to firmware test
   routines (currently just descriptive metadata).
6. Consider initializing this folder as a git repository to track firmware
   changes going forward.

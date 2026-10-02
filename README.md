# TIFF_TESTER_PRO_SD

[![GitHub Repo](https://img.shields.io/badge/GitHub-kalivoid2047%2FTIFF__TESTER__PRO__SD-181717?logo=github)](https://github.com/kalivoid2047/TIFF_TESTER_PRO_SD)
[![CI](https://github.com/kalivoid2047/TIFF_TESTER_PRO_SD/actions/workflows/ci.yml/badge.svg)](https://github.com/kalivoid2047/TIFF_TESTER_PRO_SD/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.3.0-blue.svg)](Documentation/PROJECT_STATUS.md)

Bench-first automotive module tester/diagnostic/service platform, built around
an ESP32 main controller and an independent Arduino Nano safety controller,
with a Flutter/Android companion app.

**Status: v0.3.0 (pre-1.0, bench prototype)** — dual-MCU safety architecture,
CAN/K-Line drivers, BLE service, module-profile-driven test execution, and
the mobile app are all implemented but not yet validated against real
hardware end-to-end; injector/coil driver hardware doesn't exist yet. See
[PROJECT_STATUS.md](Documentation/PROJECT_STATUS.md) for the full picture.

## Repository layout

```
TIFF_TESTER_PRO_SD/
├── Arduino_Nano_Safety/    Nano safety controller firmware
├── ESP32_Firmware/         ESP32 main application firmware
├── ESP32_Firmware_V2/      Separate Bluetooth-Classic "V2" ESP32 firmware (I2C Nano)
├── Arduino_Nano_V2/        Nano I2C output controller for the V2 firmware
├── Mobile_App/             Flutter/Android companion app
├── SD_MODULES/             Module profiles stored on the SD card
└── Documentation/          All project documentation (start here)
```

## What's implemented

- [x] Dual-MCU fail-safe architecture with independent Nano supervision
- [x] Heartbeat-based dead-man's switch (ESP32 → Nano)
- [x] Emergency stop, over/under-voltage, over-current cutoffs
- [x] Nano hardware watchdog
- [x] ESP32 Wi-Fi AP + minimal web UI
- [x] SD-based module storage with basic upload validation
- [x] One example module profile establishing the `.INI` schema
- [x] Git repository, GitHub remote, CI (compiles both sketches), Dependabot,
      MIT license, CONTRIBUTING/CODE_OF_CONDUCT, issue/PR templates
- [x] Full documentation set: PRD, SRS, Architecture, UI/UX spec,
      API/protocol spec, Roadmap
- [x] CAN driver (MCP2515) — implemented, **not hardware-validated**
- [x] K-Line driver (ISO 14230 fast-init) — implemented, **not
      hardware-validated**
- [x] `[TESTS]`/`[SERVICE]` module-profile sections consumed by firmware —
      real tests run where existing sensors make that meaningful;
      hardware-dependent tests and all `[SERVICE]` routines correctly
      report `NOT_IMPLEMENTED` rather than being faked
- [x] Test report generation to `/REPORTS` and `/LOGS`
- [x] ADC calibration mechanism (UART commands + EEPROM) — **mechanism
      implemented, actual calibration against real hardware still needed**
- [x] Relay polarity — now runtime-configurable — **actual confirmation
      against your physical relay module still needed**
- [x] BLE GATT service on the ESP32 (status/command/result, PIN-gated,
      including in-app PIN changes)
- [x] Wi-Fi AP password / BLE PIN — configurable via SD-backed config and a
      web API, no longer hard-coded
- [x] **Flutter/Android mobile app** — all ten UI_UX_SPEC.md screens,
      BLE-connected, PDF report export; analyzed/tested/built in CI

Not yet done: injector/coil driver hardware, and real-device validation of
the app against real firmware. Full detail is in
[PROJECT_STATUS.md](Documentation/PROJECT_STATUS.md).

## Documentation

Full project documentation lives in [`Documentation/`](Documentation/):

- [README.md](Documentation/README.md) — architecture overview, safety
  behavior, bench workflow, web interface
- [PROJECT_STATUS.md](Documentation/PROJECT_STATUS.md) — as-built status of
  the current firmware
- [PRD.md](Documentation/PRD.md) — product requirements
- [SRS.md](Documentation/SRS.md) — software requirements specification
- [ARCHITECTURE.md](Documentation/ARCHITECTURE.md) — system architecture
- [UI_UX_SPEC.md](Documentation/UI_UX_SPEC.md) — mobile app UI/UX spec
- [API_PROTOCOL_SPEC.md](Documentation/API_PROTOCOL_SPEC.md) — BLE/Wi-Fi/UART
  protocol contracts
- [ROADMAP.md](Documentation/ROADMAP.md) — phased development roadmap
- [BRINGUP_CHECKLIST.md](Documentation/BRINGUP_CHECKLIST.md) — hardware bring-up checklist for the bench
- [WIRING_DIAGRAM.md](Documentation/WIRING_DIAGRAM.md) — bench wiring diagrams (main system and V2) with connection tables
- [PINOUT_AND_WIRING.txt](Documentation/PINOUT_AND_WIRING.txt) — inter-
  controller and sensor wiring

## Important

The supplied firmware is a foundation for the project, not a finished OEM
diagnostic implementation. Exact connector pinouts, voltage-divider ratios,
current-sensor calibration, CAN transceiver wiring, K-Line wiring, and OEM
diagnostic/service procedures must be validated on the actual
hardware/module before connecting expensive automotive ECUs or actuators.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) — please read the safety-critical
code section before touching `Arduino_Nano_Safety/`. Participation in this
project is governed by the [Code of Conduct](CODE_OF_CONDUCT.md).

## License

MIT — see [LICENSE](LICENSE).

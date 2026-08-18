# TIFF_TESTER_PRO_SD

[![GitHub Repo](https://img.shields.io/badge/GitHub-kalivoid2047%2FTIFF__TESTER__PRO__SD-181717?logo=github)](https://github.com/kalivoid2047/TIFF_TESTER_PRO_SD)
[![Version](https://img.shields.io/badge/version-0.3.0-blue.svg)](PROJECT_STATUS.md)

Bench-first automotive module tester/diagnostic/service platform, with a
Flutter/Android companion app.

**Status: v0.3.0 (pre-1.0, bench prototype)** — see
[PROJECT_STATUS.md](PROJECT_STATUS.md) for the full implemented/remaining
breakdown.

## Controller roles

### ESP32
- Main application controller
- Wi-Fi AP / web interface
- SD module database (`/MODULES`, `/REPORTS`, `/LOGS`, `/CONFIG.INI`)
- Module upload/validation
- BLE GATT service (PIN-gated), with a real client: the Flutter app in
  `Mobile_App/`
- CAN (MCP2515) and K-Line (ISO 14230) drivers
- Module-profile-driven test execution and report generation

### Mobile app (`Mobile_App/`, Flutter/Android)
- All ten screens from [UI_UX_SPEC.md](UI_UX_SPEC.md): splash, home,
  scan, PIN connect, injector/coil/all-injectors tests, results, plus
  System Info and Settings
- Connects over BLE, drives ECU power and test commands, exports PDF
  reports
- Injector/coil test screens work end-to-end but honestly report
  `NOT_IMPLEMENTED` until driver hardware exists (Roadmap Phase 1)

### Arduino Nano
- Independent safety controller
- DUT relay control (runtime-configurable polarity)
- Voltage/current supervision (runtime-calibratable via UART commands)
- Emergency-stop supervision
- ESP32 heartbeat supervision
- Hardware watchdog

## Safety behavior

DUT power is OFF by default. The Nano removes DUT power when:
- emergency stop is pressed
- supply voltage is outside configured limits
- DUT voltage is too high
- current exceeds configured limit
- ESP32 heartbeat is lost
- Nano hardware watchdog resets the controller

## Bench workflow

1. Select module profile.
2. Verify bench wiring.
3. Run pre-power safety checks.
4. Request DUT power.
5. Nano performs independent safety checks.
6. Run module tests.
7. Diagnose communication faults if supported.
8. Run validated service/calibration procedures where supported.
9. Save test report.

## Web interface

ESP32 creates:
SSID: TIFF_TESTER
Default AP address: 192.168.4.1

The web page includes module-profile paste/upload and basic safety status.

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
[PROJECT_STATUS.md](PROJECT_STATUS.md).

## Important

The supplied firmware is a foundation for the project, not a finished OEM diagnostic implementation. Exact connector pinouts, voltage-divider ratios, current-sensor calibration, CAN transceiver wiring, K-Line wiring, and OEM diagnostic/service procedures must be validated on the actual hardware/module before connecting expensive automotive ECUs or actuators.

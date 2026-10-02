# TIFF_TESTER_PRO_SD — Project Status

_Version: 0.3.0 (pre-1.0, bench prototype) — Last updated: 2026-08-18_

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
│   ├── TIFF_TESTER_PRO_SD_ESP32.ino     ESP32 main application firmware
│   ├── shared_types.h                   Shared struct definitions
│   ├── config.ino                       SD-backed AP password / BLE PIN config
│   ├── can_mcp2515.ino                  MCP2515 CAN driver
│   ├── kline_iso14230.ino               ISO 14230 K-Line driver
│   ├── ble_service.ino                  BLE GATT service, PIN-gated
│   └── test_orchestrator.ino            Module-profile-driven test execution + reports
├── Mobile_App/                          Flutter/Android companion app
│   └── lib/
│       ├── ble/                         BLE service wrapper (flutter_blue_plus)
│       ├── models/                      NanoStatus, TestResult
│       ├── state/                       App-wide connection/results state
│       ├── screens/                     One file per UI_UX_SPEC.md screen
│       └── services/                    PDF report generation
├── SD_MODULES/
│   └── MODULES/TOYOTA/HILUX_1KD_TURBO.INI  Example module profile
├── .github/
│   ├── workflows/ci.yml                 Compiles both sketches + builds the
│   │                                     app on every push/PR
│   ├── dependabot.yml                   Weekly GitHub Actions version updates
│   ├── ISSUE_TEMPLATE/                  Bug report / safety issue / feature request
│   └── PULL_REQUEST_TEMPLATE.md
└── Documentation/                       PRD, SRS, Architecture, UI/UX spec,
                                          API/protocol spec, Roadmap, this file
```

Both firmware "sketches" compile via Arduino tooling (ESP32 board package /
AVR core); CI stages each into a folder matching its main `.ino` filename
(see `.github/workflows/ci.yml`) since Arduino requires that and this repo
organizes by controller role instead. The Flutter app is analyzed, tested,
and built (`flutter build apk --debug`) in the same CI workflow. The repo is
a git repository with GitHub Actions CI, MIT license, and standard
community-health files (see [README.md](../README.md)).

## Architecture

### ESP32 (`ESP32_Firmware/`)
- Boots a Wi-Fi Access Point (`TIFF_TESTER`) and serves a small built-in web
  UI (`WebServer` on port 80, AP IP `192.168.4.1`). The AP password and BLE
  pairing PIN are no longer hard-coded — they're read from `/CONFIG.INI` on
  SD (created with the previous defaults on first boot if missing), and
  changeable via `POST /api/config/wifi` / `POST /api/config/pin`.
- Talks to the Nano over `HardwareSerial(2)` (GPIO16/17) using a simple
  line-based CSV protocol, extended with calibration/config commands (see
  Nano section below).
- SD card (SPI, CS=GPIO5) holds `/MODULES`, `/REPORTS`, `/LOGS`, `/CONFIG.INI`.
- **CAN (`can_mcp2515.ino`)**: a self-contained, register-level MCP2515
  driver (init/send/receive) over the existing SPI bus — no external
  library dependency. Bit-timing table is pre-computed for a 500 kbps bus
  on an 8 MHz MCP2515 oscillator only; other oscillators need their CNF
  values added. **Not yet validated against a real CAN bus or module.**
- **K-Line (`kline_iso14230.ino`)**: ISO 14230 fast-init and basic
  request/response framing over a spare UART (GPIO25/26). 5-baud slow init
  is not implemented (ECU-specific timing). **Not yet validated against a
  real K-Line ECU.**
- **BLE (`ble_service.ino`)**: a GATT service (status/command/result/
  module-list/device-info characteristics) matching
  [Documentation/API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md), gated by an
  app-layer PIN check (not yet BLE link-encrypted — see that doc's §3).
  Also handles `RUN_INJECTOR_TEST`/`RUN_COIL_TEST`/`RUN_ALL_INJECTORS`
  (honestly reporting `NOT_IMPLEMENTED` — no driver hardware exists yet)
  and `SET_PIN` for in-app PIN changes. Now has a real client: the Flutter
  app in `Mobile_App/`.
- **Test orchestration (`test_orchestrator.ino`)**: parses a selected module
  profile's `[TESTS]`/`[SERVICE]` sections and runs what's actually
  measurable with the sensors this bench has today:
  - `resistance` — computed from Nano-reported DUT voltage/current (Ohm's law)
  - `short_to_ground` — flagged when current approaches the module's max
  - `current_monitor` — reports current against the module's limit
  - `position_sweep` / `actuator_movement` — return `NOT_IMPLEMENTED`
    (need injector/coil driver hardware that doesn't exist yet — see
    Roadmap Phase 1)
  - `[SERVICE]` routines (turbo calibration, DPF regen, injector learn, SCV
    learn) — read and reported, but **never executed**; always return
    `NOT_IMPLEMENTED` with an explanation. Real OEM service routines need a
    validated, per-module implementation including OEM security access,
    which is out of scope here — see [SRS.md](SRS.md).
  - Every test run appends a JSON-lines record to `/REPORTS/<module>.jsonl`
    and a line to `/LOGS/tests.log`.
- Module validation (`validModuleText`) is still minimal (presence checks,
  not full type-checking).

### Arduino Nano (`Arduino_Nano_Safety/TIFF_TESTER_PRO_SD_NANO.ino`)
- Independent fail-safe controller; relay is OFF at boot and OFF whenever
  anything looks wrong. Core safety logic (voltage/current/e-stop/heartbeat/
  watchdog) is unchanged from before.
- Accepts UART commands: `HEARTBEAT`, `POWER_ON`, `POWER_OFF`, `STATUS`,
  `RESET_FAULT`, plus new calibration/config commands:
  `CAL_SUPPLY,<v>`, `CAL_DUT,<v>`, `CAL_CURRENT_ZERO`,
  `CAL_CURRENT_SCALE,<a>`, `SET_RELAY_POLARITY,<0|1>`, `RESET_CALIBRATION`,
  `GET_CONFIG`.
- **ADC calibration and relay polarity are now runtime-configurable and
  EEPROM-persisted** instead of hard-coded constants — the commands above
  let you calibrate against a trusted reference meter and confirm relay
  polarity without recompiling. **Nobody has run this calibration
  procedure against real hardware yet** — the mechanism exists, the actual
  bench measurement is still a physical step you need to do (see
  `Documentation/PINOUT_AND_WIRING.txt`).

### Mobile app (`Mobile_App/`, Flutter/Android)
- Implements all ten reference screens from
  [UI_UX_SPEC.md](UI_UX_SPEC.md): splash, home/dashboard, scan, PIN
  connect, injector test, coil test, all-injectors test, results (with PDF
  export), plus the two screens the spec flagged as undesigned gaps
  (System Info, Settings) and a resolution for the "Tests" tab gap (a
  fuller test menu combining the channel tests with the sensor-based
  quick tests).
- **Local Vehicle & Module database (PRD.md §11 / ROADMAP.md Phase 6,
  2026-09-29)**: fully offline CRUD for vehicle and module metadata
  records, independent of BLE/firmware — `vehicle_list_screen`/
  `vehicle_form_screen`, `module_database_screen`/`module_form_screen`,
  reachable from a new Home card. Persisted via `shared_preferences`
  (`AppState`), same pattern as results persistence. Search, add, edit,
  duplicate, and delete-with-confirmation all verified on a physical
  device. Distinct from `module_selection_screen.dart`, which still reads
  the firmware's actual SD `.INI` list over BLE — the two are linked only
  by a manually-entered `sdModuleId` field, since there's no BLE command to
  write a new `.INI` file to the SD card.
- **Controls screen + relay/MOSFET labeling (PRD.md §11 / ROADMAP.md
  Phase 6, 2026-09-29)**: new `controls_screen.dart` reachable from Home's
  "FULL CONTROLS" button. Home's former "ECU Control" card (ambiguous —
  read as engine-control-unit power) is now explicitly "DUT Relay" with
  DUT ON/DUT OFF, matching what `POWER_ON`/`POWER_OFF` actually drive (the
  one physical relay, per `Arduino_Nano_Safety/TIFF_TESTER_PRO_SD_NANO.ino`
  `RELAY_PIN`). Also added a `RESET FAULT` button (the `AppState.resetFault()`
  method already existed but had no UI). Auxiliary Relay (RELAY 2–4) and
  MOSFET (MOSFET 1–2) buttons are shown but disabled with an explanatory
  snackbar, since no auxiliary relay/MOSFET driver hardware or firmware
  command exists yet (Roadmap Phase 1) — deliberately not wired to send a
  command `ble_service.ino` would silently drop. New
  `AppState.allOutputsOff()` sends both `POWER_OFF` and `STOP_TEST` (the
  two PIN-auth-exempt commands); a red, icon-marked "ALL OUTPUTS OFF"
  button appears on both Home and Controls, requires no confirmation
  dialog (a deliberate exception — everything else destructive in the app
  does confirm), and is verified to show "Connect to a device first" with
  no unhandled exceptions when disconnected.
- BLE via `flutter_blue_plus`: scan, connect, PIN auth, live status
  notifications (~250ms cadence from firmware), command writes, result
  notifications — speaks the exact text-command protocol
  `ble_service.ino` implements (documented in
  [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md) §1.3).
- Injector/Coil/All-Injectors test screens work end-to-end against real
  firmware today — they just honestly display `NOT_IMPLEMENTED` as the
  result, since the driver hardware doesn't exist. No app changes will be
  needed when Roadmap Phase 1 hardware lands; only the firmware's answer
  changes.
- PDF report export via `pdf`/`printing` packages, matching
  [SRS.md](SRS.md) §6.3.
- Verified: `flutter analyze` (0 issues), `flutter test` (passing),
  `flutter build apk --debug` (succeeds) — all three run in CI.
- Not yet done: real device testing against physical firmware (only
  verified to compile/run in isolation), iOS (out of scope per PRD), and
  persisting results across app restarts (session-only for now).

### Module profile format (`SD_MODULES/MODULES/TOYOTA/HILUX_1KD_TURBO.INI`)
Unchanged schema. `[TESTS]` and `[SERVICE]` sections are now actually
consumed by the ESP32 firmware (see above), not just stored as text.

## Wiring (see `Documentation/PINOUT_AND_WIRING.txt`)

| Signal | ESP32 | Nano |
|---|---|---|
| Inter-MCU UART | GPIO16 (RX2) / GPIO17 (TX2) | D0/D1 |
| SD card CS | GPIO5 | — |
| CAN CS / INT | GPIO15 / GPIO4 (INT wired, not yet used — driver polls) | — |
| K-Line UART | GPIO25 (RX) / GPIO26 (TX) | — |
| DUT voltage ADC | GPIO34 | A0 |
| DUT current ADC | GPIO35 | A2 |
| Supply voltage ADC | GPIO32 | A1 |
| DUT relay driver | — | D4 |
| Emergency stop | — | D3 (INPUT_PULLUP, normally-closed contact to GND, fail-safe on wire break) |
| Buzzer | — | D6 |
| Status LED | — | D13 |
| Temp/spare | — | A3 |

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
- [x] `[TESTS]`/`[SERVICE]` sections consumed by firmware — real tests run
      where existing sensors make that meaningful; hardware-dependent tests
      and all `[SERVICE]` routines correctly report `NOT_IMPLEMENTED`
      rather than being faked
- [x] Test report generation — `/REPORTS/*.jsonl` and `/LOGS/tests.log` now
      written on every test run
- [x] ADC calibration mechanism (UART commands + EEPROM) — **mechanism
      implemented, actual calibration against real hardware still needed**
- [x] Relay polarity — now runtime-configurable — **actual confirmation
      against your physical relay module still needed**
- [x] BLE GATT service on the ESP32 (status/command/result, PIN-gated),
      including `SET_PIN` and `RUN_INJECTOR_TEST`/`RUN_COIL_TEST`/
      `RUN_ALL_INJECTORS`
- [x] Wi-Fi AP password / BLE PIN — configurable via SD-backed config and a
      web API, no longer hard-coded (still ship with the same default
      values as before, which you should change)
- [x] **Android mobile app (Flutter)** — all ten UI_UX_SPEC.md screens
      implemented, BLE-connected, PDF report export; `flutter analyze`/
      `flutter test`/`flutter build apk --debug` all pass in CI. Injector/
      coil test screens are real end-to-end but honestly report
      `NOT_IMPLEMENTED` pending Roadmap Phase 1 hardware; not yet tested
      against a physical device or real ESP32 over the air.

## What's still not implemented / needs physical hardware work

- [ ] **Injector/coil driver hardware** — doesn't exist yet; `position_sweep`
      and `actuator_movement` tests, and the mobile app's injector/coil test
      screens, depend on this. See [Roadmap.md](ROADMAP.md) Phase 1.
- [ ] **Real device testing of the mobile app** — built and unit-verified,
      but nobody has run it on an actual phone against actual firmware yet.
- [ ] **Real ADC calibration values** — the CAL_* commands exist; nobody has
      run them against a trusted reference meter on real hardware.
- [ ] **Real relay polarity confirmation** — `SET_RELAY_POLARITY` exists;
      nobody has confirmed which value is correct against your actual relay
      module.
- [ ] **CAN/K-Line hardware validation** — both drivers are implemented but
      untested against a real MCP2515 board / K-Line ECU. Bit-timing
      assumes an 8 MHz MCP2515 oscillator.
- [ ] **BLE link-layer encryption/bonding** — current auth is app-layer PIN
      only (Option A in API_PROTOCOL_SPEC.md §3); fine for a bench tool,
      not field-hardened.
- [ ] **RTC / real timestamps** — report timestamps are currently
      `uptime_ms`, not wall-clock time (no RTC on the board yet).
- [ ] Hardware-in-the-loop automated testing (CI only compile-checks the
      firmware; it can't exercise real Nano/ESP32/sensor behavior).

## Confirmed resource-pressure finding

Combining Wi-Fi AP + WebServer + SD + BLE + CAN + K-Line on the ESP32
**does exceed the default partition scheme's app space** — this is the
exact risk flagged in [ARCHITECTURE.md](ARCHITECTURE.md) §6, and it's now
confirmed (CI failed with "text section exceeds available space in board"
until fixed). Resolved by switching to the `huge_app` partition scheme
(trades away OTA space, which this bench tool doesn't use, for a larger
app partition) — see `.github/workflows/ci.yml` and the board-setting note
at the top of `TIFF_TESTER_PRO_SD_ESP32.ino`. If you're flashing via the
Arduino IDE rather than CI, you must set this manually
(Tools > Partition Scheme > "Huge APP") or the build will fail the same
way.

## Suggested next steps

1. Physically calibrate the Nano's ADC scaling using the new `CAL_*`
   commands against a trusted multimeter/ammeter, and confirm relay
   polarity with `SET_RELAY_POLARITY`.
2. Bench-test the CAN driver against a real MCP2515 board and a known CAN
   device (even a simple loopback/second node) before pointing it at a
   vehicle module.
3. Bench-test the K-Line driver against a real ECU with a scope on the
   K-Line signal to confirm fast-init timing actually wakes it up.
4. Change the default AP password and BLE PIN via the new config API
   before any use beyond an isolated bench.
5. Start on the injector/coil driver hardware (Roadmap Phase 1) — this
   unblocks the two `NOT_IMPLEMENTED` test types and the mobile app's core
   test screens.
6. Install the app on a real Android phone and pair it with a real ESP32
   running this firmware — confirm the scan/connect/PIN/status/test flow
   actually works over the air, not just in isolation.

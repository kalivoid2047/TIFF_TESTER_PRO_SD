# System Architecture
## TIFF TESTER PRO

_Version: 0.1 (Draft) — 2026-08-18_
_Parent document: [PRD.md](PRD.md) · Requirements: [SRS.md](SRS.md)_

---

## 1. Component overview

```
┌─────────────────────────┐        BLE (GATT, PIN-gated)        ┌───────────────────────────────────────┐
│   Android App            │ <----------------------------------> │   ESP32 Main Controller                │
│   "TIFF TESTER PRO"      │                                      │                                         │
│                          │        Wi-Fi AP + HTTP (bench/eng)   │  - BLE peripheral (NimBLE)              │
│  - BLE scan/pair         │ <----------------------------------> │  - Wi-Fi AP + WebServer (existing)      │
│  - Dashboard / ECU ctrl  │                                      │  - SD card: /MODULES /REPORTS /LOGS     │
│  - Injector/Coil tests   │                                      │  - Module profile parser/validator      │
│  - Results + PDF export  │                                      │  - Test orchestrator (new)              │
└─────────────────────────┘                                      │  - CAN (MCP2515) / K-Line (future)       │
                                                                   └───────────────┬─────────────────────────┘
                                                                                   │ UART (existing line protocol,
                                                                                   │ extended for injector/coil cmds)
                                                                                   ▼
                                                                   ┌───────────────────────────────────────┐
                                                                   │   Arduino Nano Safety Controller        │
                                                                   │                                         │
                                                                   │  - Independent safety supervision       │
                                                                   │  - DUT relay + injector/coil driver      │
                                                                   │    enable lines (new channels)          │
                                                                   │  - Voltage/current ADC monitoring        │
                                                                   │  - E-stop, heartbeat watchdog, HW WDT    │
                                                                   └───────────────┬─────────────────────────┘
                                                                                   │ drive signals (PWM/GPIO,
                                                                                   │ opto-isolated recommended)
                                                                                   ▼
                                                                   ┌───────────────────────────────────────┐
                                                                   │   Driver stage (new hardware)           │
                                                                   │  - 4x injector low-side drivers          │
                                                                   │  - 4x coil dwell drivers                 │
                                                                   │  - Per-channel current sense             │
                                                                   │  - Flyback protection                    │
                                                                   └───────────────┬─────────────────────────┘
                                                                                   ▼
                                                                             DUT (injector / coil)
```

## 2. Why BLE **and** Wi-Fi coexist

The screenshots specify BLE as the app's transport (scan → PIN pair →
control). The existing firmware already implements a Wi-Fi AP + web UI. Both
are kept because they solve different problems:

| | BLE | Wi-Fi AP + HTTP |
|---|---|---|
| Primary consumer | Mobile app (v1 target) | Browser, bench debugging, SD/module management |
| Power/complexity | Lower power, simpler pairing model, better suited to a handheld companion app | Already implemented; useful for uploading module profiles from a laptop, viewing raw status without installing an app |
| Bandwidth | Sufficient for status/commands/small result payloads | Better for larger transfers (module profile text, firmware-adjacent debugging) |

**Decision (default, revisit if ESP32 memory/coexistence testing says
otherwise — see SRS §8 open item 2):** Wi-Fi AP remains always-on for
bench/engineering use; BLE is added as a second, independent interface for
the app. Both read the same underlying state (`NanoStatus`, module list) —
neither is authoritative over the other, and neither is authoritative over
safety (Nano is).

## 3. Safety boundary (unchanged principle, extended surface)

The core safety principle from the existing Nano firmware is preserved and
must not be weakened by any addition in this roadmap:

> **The Nano is the only component that can energize a DUT-facing output,
> and it only does so after its own independent checks pass — regardless of
> what any other layer (ESP32, BLE, Wi-Fi, the app, or a malformed/malicious
> command) requests.**

Extending from a single relay to injector/coil driver channels means the
Nano's responsibility grows from "one relay" to "N channels, each
individually enabled only within safe parameters, all channels forced OFF
under the existing fault conditions (e-stop, under/over-voltage, over-DUT-
voltage, over-current, heartbeat loss, watchdog reset)." No new channel may
bypass this supervisor — see [SRS.md](SRS.md) FR-SAFE-1..5.

Pulse-width/dwell-time hard caps (FR-INJ-4, FR-COIL-4) live on the Nano side
(or are cross-checked there), not only in app/ESP32 logic, so that a bad
value from the app can't reach hardware unchecked.

## 4. Data flow — example: run an injector test

1. App (paired, PIN-authenticated) sends `RUN_INJECTOR_TEST` command over
   BLE with `{channel, pulse_width_ms, duration_s}`.
2. ESP32 validates the request against the currently selected module
   profile's `[TEST_INJECTOR]` limits (if any) and its own sanity bounds,
   then forwards a UART command to the Nano.
3. Nano re-validates against its own hard-coded max pulse width/duty cycle,
   checks current safety state (voltage/current/e-stop all OK), and only
   then pulses the requested driver channel, sampling current per pulse.
4. Nano streams status/telemetry back over UART (extended `STATUS`/new
   `TEST_STATUS` lines).
5. ESP32 relays progress to the app over BLE notify (status characteristic)
   at the existing ~250 ms cadence.
6. On completion, ESP32 assembles a result record (SRS §6.2), sends a final
   notify, and writes the record to `/REPORTS` on SD.
7. App appends the result to the in-session results list and updates the UI
   (screenshots 9 → 10).
8. Technician taps "Save PDF Report"; app renders a PDF from the session's
   result records.

## 5. Module boundaries / repo layout (target state)

```
TIFF_TESTER_PRO_SD/
├── Arduino_Nano_Safety/
│   └── TIFF_TESTER_PRO_SD_NANO.ino        existing + injector/coil channel supervision
├── ESP32_Firmware/
│   └── TIFF_TESTER_PRO_SD_ESP32.ino       existing + BLE service + test orchestrator
├── Mobile_App/                             NEW — Android app source
│   └── (Kotlin/Java, per ROADMAP Phase 2)
├── Hardware/                               NEW — driver-stage schematics/BOM
│   └── (KiCad or similar, per ROADMAP Phase 1)
├── SD_MODULES/
│   └── MODULES/...                         existing + [TEST_INJECTOR]/[TEST_COIL] sections
└── Documentation/
    ├── README.md
    ├── PINOUT_AND_WIRING.txt
    ├── PROJECT_STATUS.md
    ├── PRD.md
    ├── SRS.md
    ├── ARCHITECTURE.md   (this file)
    ├── UI_UX_SPEC.md
    ├── API_PROTOCOL_SPEC.md
    └── ROADMAP.md
```

## 6. Key architectural risks

| Risk | Mitigation |
|---|---|
| ESP32 RAM/flash pressure running Wi-Fi AP + NimBLE + WebServer + SD concurrently | Prototype the coexistence early (Roadmap Phase 1); fall back to Wi-Fi-on-demand if unstable |
| New driver-stage hardware introduces a safety gap if not routed through Nano | Architectural rule in §3 is non-negotiable; code review checklist item in Roadmap Phase 3 |
| BLE PIN pairing implemented ad-hoc (not standard BLE bonding) could be weaker than it looks | Evaluate standard BLE bonding/LE Secure Connections vs. an app-layer PIN check; document choice in API_PROTOCOL_SPEC §3 |
| Module profile format drifts between what ESP32 validates and what app expects | Single schema owned in this repo (`SD_MODULES`), versioned; both firmware and app parse against the same documented schema (SRS §6.1) |

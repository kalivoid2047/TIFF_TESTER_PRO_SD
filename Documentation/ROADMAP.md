# Development Roadmap
## TIFF TESTER PRO — Full System

_Version: 0.3 (Draft) — 2026-09-29_
_Rolls up: [PRD.md](PRD.md), [SRS.md](SRS.md), [ARCHITECTURE.md](ARCHITECTURE.md),
[UI_UX_SPEC.md](UI_UX_SPEC.md), [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md)_

This roadmap takes the project from its current state
([PROJECT_STATUS.md](PROJECT_STATUS.md): BLE-connected ESP32/Nano bench
firmware with a working Flutter mobile app, CAN and K-Line drivers
implemented but not hardware-validated, and no injector/coil driver
hardware yet) to the full product described in the PRD. Phases are ordered
by dependency, not calendar time — treat durations as relative-effort
placeholders to be reforecast once a team size is known.

Phases 0–5 deliver the **bench injector/coil tester** (PRD §7.1–7.2). A
2026-09-29 gap review against the client's separate full-system vision
brief ("TIFF TESTER PRO app data.doc", 53 sections — see
[PRD.md](PRD.md) §11) found the as-built app does not, and was never
scoped to, cover that brief's full multi-brand diagnostic platform. Phase 6
below tracks that larger scope as explicit backlog instead of leaving it
implicit.

---

## Phase 0 — Decisions & documentation lock (this deliverable)

**Goal:** stop building on assumptions; get the open questions answered.

- [x] PRD, SRS, Architecture, UI/UX spec, API/protocol spec drafted (this
      batch of documents).
- [x] Git version control initialized, pushed to a GitHub remote, with CI
      (compiles both sketches + builds the app), Dependabot, MIT license,
      and community-health files — see [PROJECT_STATUS.md](PROJECT_STATUS.md).
- [x] BLE auth: Option A (app-layer PIN check) chosen and implemented in
      `ble_service.ino`; link-layer encryption/bonding explicitly deferred
      (see Phase 4).
- [x] Android minimum SDK: 26, implemented in the Flutter app.
- [x] PDF library choice: `pdf`/`printing` packages, implemented.
- [x] Wi-Fi-always-on vs. on-demand alongside BLE: decided always-on —
      the Phase 2 coexistence test confirmed Wi-Fi AP + WebServer + SD +
      BLE + CAN + K-Line run concurrently (after the `huge_app` partition
      fix), so there's no resource-pressure reason to add on-demand
      Wi-Fi toggling. Revisit only if that changes.
- [ ] Injector/coil driver circuit topology (needs a hardware engineer's
      input, not just firmware) — still open, blocks Phase 1.
- [x] Filled the UI gaps flagged in UI_UX_SPEC §6 that didn't need a new
      reference design: System Info, Settings, and module-selection
      screens (first-pass), Tests tab (resolved as a fuller test menu).
      Empty/error states beyond the basics are still open — see Phase 3.

**Exit criteria:** every "TBD"/"open item" in the docs above has an owner
and an answer, or an explicit "defer to Phase N" note. Only the driver
topology (needs a hardware engineer, not firmware/app work) and
empty-state UI polish remain open.

---

## Phase 1 — Hardware: injector/coil driver stage

**Goal:** the single biggest net-new piece of hardware this product needs
and doesn't have yet.

- [ ] Select injector driver topology (e.g. low-side MOSFET + flyback diode
      per channel, or a purpose-built injector driver IC) and coil driver
      topology (dwell-controlled primary switching, appropriate for
      inductive ignition coil loads — higher voltage/energy than an
      injector channel).
- [ ] Add per-channel current sensing suitable for both pass/fail
      evaluation (resistance/short-to-ground per the existing module
      schema's `[TEST_RESISTANCE]`) and live telemetry during a pulse.
- [ ] Confirm isolation/protection between DUT-side voltages and Nano GPIO
      (existing wiring doc already flags this principle for the relay;
      extend it explicitly to the new channels).
- [ ] Prototype on breadboard/perfboard; validate against a real injector
      and a real ignition coil on the bench with a scope before any
      software integration.
- [ ] Update [PINOUT_AND_WIRING.txt](PINOUT_AND_WIRING.txt) with the new
      channel pin assignments on the Nano (and ESP32 if any driver status
      lines route there directly).

**Exit criteria:** a bench prototype can safely fire one injector channel
and one coil channel under manual (non-app) control, with current sensing
confirmed accurate against a reference meter.

**Depends on:** Phase 0 (driver topology decision).

---

## Phase 2 — Firmware: safety + orchestration extension

**Goal:** extend the existing, working safety architecture to the new
channels without weakening it.

**Status: the general BLE/orchestration/reporting plumbing landed ahead of
schedule (independent of driver hardware); the injector/coil-specific
pieces are still blocked on Phase 1 hardware.**

- [x] ESP32: BLE peripheral added, GATT service from
      [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md) §1 implemented in
      `ble_service.ino`, gated by the Option A PIN auth from Phase 0
      (link-layer encryption still deferred — see Phase 4).
- [x] ESP32: test-orchestration layer (`test_orchestrator.ino`) validates
      module-profile-driven commands and runs what's measurable today
      (`resistance`, `short_to_ground`, `current_monitor`); `position_sweep`
      and `actuator_movement` correctly return `NOT_IMPLEMENTED` pending
      Phase 1 hardware rather than being faked.
- [x] ESP32: `/REPORTS/<module>.jsonl` + `/LOGS/tests.log` writer
      implemented, producing a result record on every test run.
- [x] CAN driver (`can_mcp2515.ino`, register-level MCP2515) and K-Line
      driver (`kline_iso14230.ino`, ISO 14230 fast-init) implemented —
      **not yet hardware-validated against a real bus/ECU** (this work was
      originally deferred to Phase 5; it landed early but still needs the
      bench validation described there).
- [x] Coexistence: Wi-Fi AP + WebServer + SD + BLE + CAN + K-Line run
      concurrently on target ESP32 hardware — required switching to the
      `huge_app` partition scheme after CI caught a "text section exceeds
      available space" build failure (see
      [PROJECT_STATUS.md](PROJECT_STATUS.md) "Confirmed resource-pressure
      finding").
- [x] `README.md`/`PROJECT_STATUS.md` updated to reflect BLE + CAN/K-Line +
      orchestrator + mobile app landing.
- [x] Nano: `INJ_TEST`/`COIL_TEST`/`TEST_STOP` command handling added
      (`startTestWindow()`/`stopTestWindow()`), with the requested
      pulse-width/dwell rejected outright (not silently clamped) against a
      hard-coded `MAX_PULSE_WIDTH_MS`/`MAX_DWELL_MS`, independent of what
      the ESP32 sends.
- [x] Nano: `safetyOK()`-style supervision now runs continuously for the
      whole duration of a test window (`testActive` state checked every
      `loop()` iteration), not just once before it starts — mirrors
      `relayOnSafe()`'s recheck-after-energize pattern but held open.
      Still safety-supervision-only: no injector/coil driver GPIO exists to
      actually pulse (Phase 1), so a completed window reports
      `NOT_IMPLEMENTED` rather than a fabricated PASS/FAIL.
- [x] Nano: status reporting extended with `TEST_STATUS,<type>,<channel>,
      <status>,<elapsed_s>,<last_current_a>,<result>,<faulttext>` lines per
      API_PROTOCOL_SPEC.md §4.3.
- [x] ESP32: module `.INI` schema extended with `[TEST_INJECTOR]`/
      `[TEST_COIL]` sections (SRS §6.1) — `iniSection()` added to
      `test_orchestrator.ino` so same-named keys (`enabled=`) in different
      sections no longer collide in the flat key scanner; `ModuleProfile`
      gained the corresponding fields; `HILUX_1KD_TURBO.INI` updated
      (disabled — it's a turbo actuator, not an injector/coil module).
      Additional example profiles for other vehicle families not yet
      added.

**Exit criteria:** a BLE client can pair with a PIN, request an injector
pulse test, and receive status/result notifications, with the Nano safety
layer still the sole authority over the driver outputs. Currently true for
the pairing/status/reporting path; the injector/coil pulse itself still
depends on Phase 1 hardware.

**Depends on:** Phase 1 (hardware to drive), Phase 0 (protocol/auth
decisions — resolved).

---

## Phase 3 — Mobile app: core flow

**Goal:** build the Android app to match the ten reference screens.

**Status: built (Flutter, not Kotlin — see note below), CI-verified, not
yet run against real hardware.**

- [x] Project setup — built with **Flutter/Dart** instead of the
      originally-planned Kotlin (faster to get a working cross-widget UI
      matching the reference screenshots exactly; revisit only if a native
      Android-specific need arises). Min SDK 26 per Phase 0/SRS
      NFR-COMPAT-1.
- [x] BLE scan/connect/pair flow (screens 3–4): runtime BLE/location
      permissions (Android 12+ model), device list with RSSI, PIN entry.
      Failure/backoff *display* states not yet built (firmware-side
      lockout exists; app doesn't yet surface "locked out, retry in Ns").
- [x] Home/Connected dashboard (screens 2/5): connection status, ECU
      on/off, quick-test tiles, bottom nav shell.
- [x] Injector test screen (screens 6/9): channel select, parameter
      inputs, start/stop, live status. Honestly reports `NOT_IMPLEMENTED`
      pending Phase 1 hardware rather than faking a result.
- [x] Coil test screen (screen 7): same pattern, coil-specific fields.
- [x] All Injectors test screen (screen 8): request + progress display;
      firmware currently answers as one combined result rather than a true
      4-step sequence (no driver hardware to actually sequence yet).
- [x] Results screen (screen 10): session result list, clear (with confirm
      dialog), PDF export via `pdf`/`printing` packages.
- [x] Splash screen (screen 1).
- [x] Filled the UI gaps from UI_UX_SPEC §6: System Info and Settings
      screens built (first-pass, no reference design existed); Tests tab
      resolved as a fuller test menu; module-selection screen now built
      (see below). Empty/error states beyond the basics still open.
- [x] Local persistence for in-session results across app restarts (SRS
      NFR-REL-1) — `TestResult.toJson()`/`fromJson()` added, `AppState`
      now persists/restores the full results list (capped at 200 entries)
      via `shared_preferences`, replacing the previous count-only stub.
- [x] Module-profile selection screen (`module_selection_screen.dart`) —
      reads the BLE modules characteristic (`readModuleList()` in
      `tiff_ble_service.dart`, newly wired up; firmware's
      `bleModulesChar` now actually populated via `moduleListText()`,
      shared with the existing `/api/modules` web handler) and calls
      `selectModule()`. Reachable from a new "Module" card on the Home
      screen.
- [x] Default-PIN warning banner on the Home screen (`usingDefaultPin` in
      `AppState`, set when the PIN used to authenticate equals
      `AppState.defaultPin`) — tapping it opens Settings to change the
      PIN. Mirrors the firmware's existing `ble_pin_is_default` flag on
      the Wi-Fi debug API (`handleConfigStatus()` in `config.ino`), which
      the BLE-only app path had no equivalent for.

**Exit criteria (not yet met):** a technician can complete the full flow —
scan, pair, run one injector test and one coil test, view results, save a
PDF — against real Phase 2 firmware on real Phase 1 hardware. Currently
true only against Phase 2 firmware (no Phase 1 hardware exists, so
injector/coil tests report `NOT_IMPLEMENTED` by design) and only verified
via `flutter analyze`/`flutter test`/`flutter build apk --debug` in CI —
**nobody has run the app on a phone against a real ESP32 yet.**

**Depends on:** Phase 2 (done — firmware/protocol exists).

---

## Phase 4 — Integration, validation & safety sign-off

**Goal:** confirm the whole stack behaves safely and correctly end to end
before calling this a usable bench product.

- [ ] End-to-end test matrix: e-stop during a running test, heartbeat loss
      during a running test, over-current during a running test, BLE
      disconnect during a running test — confirm DUT power/driver outputs
      go OFF within the latency target (SRS NFR-SAFE-1, <100 ms) in every
      case.
- [ ] Validate injector/coil test results against a known-good and a
      known-bad component per module profile, confirm pass/fail thresholds
      are meaningful (not just "code runs").
- [ ] Confirm report data written to SD and the app's PDF agree, for the
      same test session.
- [ ] Security pass on BLE auth (link-encryption decision from Phase
      0/API spec §3 still open). Rate-limiting (`blePinFailCount`/
      `blePinLockoutUntil` backoff in `ble_service.ino`) and a default-PIN
      warning (firmware-side in `config.ino`'s boot/`/api/config` log +
      status, app-side via the Phase 3 Home screen banner above) already
      exist — this item is now scoped to the link-encryption/bonding
      decision and any follow-on hardening it implies.
- [ ] Update all module `.INI` profiles and this documentation set to match
      whatever changed during implementation (docs are a living set, not a
      one-time deliverable).

**Exit criteria:** the "Success metrics" table in [PRD.md](PRD.md) §8 is
measured and met (or explicitly re-baselined with reasons).

**Depends on:** Phase 3.

---

## Phase 5 — Enclosure & field-readiness (stretch)

**Goal:** move from bench prototype to a unit that survives a workshop
environment.

- [ ] Enclosure design for ESP32 + Nano + driver stage + connectors.
- [ ] Connectorized DUT harness (vs. current breadboard-style wiring).
- [ ] Power input protection (reverse polarity, transient suppression) —
      not called out in current Nano firmware beyond ADC monitoring.
- [ ] Expand module profile library beyond the 5-family v1 target (PRD §8).
- [ ] Bench-validate the CAN (`can_mcp2515.ino`) and K-Line
      (`kline_iso14230.ino`) drivers against a real MCP2515 board / real
      K-Line ECU — implemented in Phase 2 ahead of schedule but never run
      against real hardware; this is where `[COMMUNICATION]`/`[SERVICE]`
      sections of the module schema finally get consumed, under the same
      "validate before implementing OEM routines" caution already stated
      in the existing firmware headers.
- [ ] Add RTC for real wall-clock report timestamps (currently `uptime_ms`).

**Depends on:** Phase 4.

---

## Phase 6 — Full multi-brand diagnostic platform (vision alignment, stretch)

**Goal:** grow from a single-purpose bench injector/coil tester into the
multi-brand vehicle diagnostic platform described in the client's vision
brief ("TIFF TESTER PRO app data.doc" — see [PRD.md](PRD.md) §11). Nothing
here is required for Phases 0–5 to ship; this is where the app goes *after*
the bench tool works end to end on real hardware.

- [x] **Vehicle & module database CRUD** (brief §13–§16) — local, offline
      Vehicle and Module databases (`lib/models/vehicle.dart`,
      `lib/models/module_profile.dart`), persisted via `shared_preferences`
      (`AppState` add/update/delete/duplicate methods, mirroring the
      existing results-persistence pattern). Screens: `vehicle_list_screen`/
      `vehicle_form_screen` and `module_database_screen`/
      `module_form_screen`, reachable from a new Home "Vehicle & Module
      Database" card — search, add, edit, duplicate, delete-with-
      confirmation all verified on-device. The module form's Vehicle field
      links to a vehicle record; `ModuleProfile.sdModuleId` is a separate,
      manually-entered link to the firmware's actual SD `.INI` file (there
      is still no BLE command to write a new `.INI` to the SD card, so this
      database does not yet replace `module_selection_screen.dart`'s
      firmware-backed list — it complements it). Cascading
      Manufacturer→Model→Year→Engine→Module *selection* (as opposed to
      *management*) inside the test flow is not yet wired up — still open.
- [ ] **CAN / K-Line diagnostics UI** (brief §6, §11, §51) — status cards
      (online/offline, speed, TX/RX IDs, protocol) plus a Diagnostics screen
      (READ STATUS / READ FAULTS / CLEAR FAULTS / RESET MODULE). Depends on
      the CAN/K-Line drivers finally getting hardware-validated (Phase 5).
- [ ] **Live data graphs** (brief §10) — real-time voltage/current/power/
      position/temperature charts, parameter selector, start/stop/clear.
- [ ] **SD-logging UI** (brief §6, §20–§21) — SD status card, start/stop
      logging, view/export/delete log files from within the app.
- [x] **Relay/MOSFET-granular manual control** (brief §17–§18, §37) — new
      `controls_screen.dart`: DUT Relay section (state, DUT ON/OFF, Reset
      Fault — the one relay that actually exists, now labeled explicitly as
      "DUT Relay" instead of the previous ambiguous "ECU Control"/"ECU
      ON/OFF"), Auxiliary Relays (RELAY 2–4) and MOSFET Outputs (MOSFET
      1–2) sections shown disabled with an explanatory snackbar ("Needs
      driver hardware not yet installed (Roadmap Phase 1)") rather than
      wired to a command the firmware would silently ignore — no
      `ble_service.ino` command exists for these yet, unlike the
      injector/coil tests' honest `NOT_IMPLEMENTED` path. An always-visible
      "ALL OUTPUTS OFF" (`AppState.allOutputsOff()`, sends `POWER_OFF` +
      `STOP_TEST`, both PIN-auth-exempt per `ble_service.ino`) is on both
      Home and the Controls screen, requires no confirmation dialog per
      brief §41, and is styled distinctly (danger-red, icon) from the
      ordinary DUT OFF button. Verified on-device: correct "Connect to a
      device first" gating while disconnected, no unhandled exceptions.
- [ ] **Communication Monitor** (brief §33) — raw TX/RX command log screen
      with clear/start/stop, for development and field troubleshooting.
- [ ] **Access levels** (brief §36) — Technician / Advanced-Engineer /
      Administrator modes; gate raw CAN/K-Line/relay/MOSFET access and
      configuration behind Advanced/Admin.
- [ ] **Error History screen** (brief §42) — persisted fault log (date/time,
      type, voltage/current, module, action taken), separate from Results.
- [ ] **Step-by-step test procedure wizard + module-specific dynamic
      controls** (brief §43–§44) — Previous/Next/Start driven by a
      per-module procedure definition; render position %/RPM/PWM %/
      open-close controls from the module definition instead of the
      current hardcoded pulse-width/duration sliders.
- [ ] **Richer connection-error handling** (brief §34) — reason-specific
      messaging (device off / Bluetooth disabled / lost / timeout) with
      Reconnect / Scan Again / Open Bluetooth Settings actions.
- [ ] **Global search** (brief §26) across vehicles/modules/results/faults/
      logs.
- [ ] **Technician notes field** (brief §25) — free-text observation saved
      per test result.
- [ ] **Database backup/restore, module/vehicle export-import** (brief §31)
      — transfer a module/vehicle database between phones.
- [ ] **About screen + full navigation surface** (brief §4) — grow the
      current 4-tab shell (Home/Tests/Results/Settings) toward the brief's
      13-item nav (Dashboard/Connect/Diagnostics/Module Tester/Controls/
      Live Data/SD Logging/Modules/Vehicles/Test Results/Reports/Settings/
      About) as the screens above land — don't restructure navigation
      before there's content to justify it.
- [ ] **Delete confirmations / pre-delete backup** (brief §46) for the new
      database CRUD screens above.

**Exit criteria:** the app's feature set matches the vision brief's §50
"most important requirements" checklist in full (today: roughly items 1–3,
13, 17–19, 29–30 of 37 are met — see [PRD.md](PRD.md) §11 for the mapping).

**Depends on:** Phase 4 (validated bench tool) and, for the CAN/K-Line UI
item specifically, Phase 5's hardware validation.

---

## Dependency graph

```
Phase 0 (decisions/docs)
   │
   ├──▶ Phase 1 (driver hardware) ──▶ Phase 2 (firmware) ──▶ Phase 3 (mobile app) ──▶ Phase 4 (validation) ──▶ Phase 5 (field-ready) ──▶ Phase 6 (full platform, stretch)
   │
   └──▶ (UI-only work in Phase 3 can start early against a mocked BLE service, in parallel with Phase 1/2)
```

## Risk register (top items, see individual docs for full detail)

| Risk | Impact | Phase | Mitigation |
|---|---|---|---|
| Injector/coil driver hardware undersized or unsafe | High — damages DUTs or is unsafe | 1 | Bench-validate with scope before firmware integration |
| ESP32 resource exhaustion running Wi-Fi+BLE+SD+Web concurrently | Medium — forces architecture rework | 2 | Early coexistence test, Wi-Fi-on-demand fallback ready |
| BLE PIN auth weaker than intended | Medium — security | 2/4 | Explicit Option A/B decision + link encryption + rate limiting |
| Module `.INI` schema drifts between firmware and app | Medium — silent bugs | 2/3 | Single schema doc (SRS §6.1), both sides implement against it |
| No reference design for 4 of the UI screens | Low/Medium — rework risk | 3 | Close UI_UX_SPEC §6 gaps in Phase 0 before building those screens (System Info/Settings/Tests tab done; module-selection screen + empty/error states still open) |
| CAN/K-Line drivers implemented but never run against real hardware | Medium — could be wrong (bit-timing, framing) once tested | 5 | Bench-validate against a real MCP2515 board and K-Line ECU before any vehicle use |
| Stakeholder expects the full multi-brand platform (vision brief) sooner than Phase 6 | Medium — expectation mismatch, not a code defect | 0/6 | PRD §11 + this Phase 6 make the gap and sequencing explicit; revisit priority with stakeholder if Phase 6 needs to move earlier |

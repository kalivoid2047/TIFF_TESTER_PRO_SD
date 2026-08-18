# Development Roadmap
## TIFF TESTER PRO — Full System

_Version: 0.1 (Draft) — 2026-08-18_
_Rolls up: [PRD.md](PRD.md), [SRS.md](SRS.md), [ARCHITECTURE.md](ARCHITECTURE.md),
[UI_UX_SPEC.md](UI_UX_SPEC.md), [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md)_

This roadmap takes the project from its current state
([PROJECT_STATUS.md](PROJECT_STATUS.md): bench Wi-Fi-only firmware, no
mobile app, no injector/coil driver hardware) to the full product described
in the PRD. Phases are ordered by dependency, not calendar time — treat
durations as relative-effort placeholders to be reforecast once a team size
is known.

---

## Phase 0 — Decisions & documentation lock (this deliverable)

**Goal:** stop building on assumptions; get the open questions answered.

- [x] PRD, SRS, Architecture, UI/UX spec, API/protocol spec drafted (this
      batch of documents).
- [ ] Resolve open items called out in SRS §8 and PRD §9:
  - Wi-Fi-always-on vs. on-demand alongside BLE.
  - BLE auth: Option A vs. B (API_PROTOCOL_SPEC §3).
  - Android minimum SDK.
  - PDF library choice.
  - Injector/coil driver circuit topology (needs a hardware engineer's
    input, not just firmware).
- [ ] Fill UI gaps flagged in UI_UX_SPEC §6 (System Info, Settings, Tests
      tab, empty/error states, module-selection screen) with real reference
      designs or explicit sign-off to design them fresh.
- [ ] Initialize git version control for this repository (currently none),
      so all following phases are tracked.

**Exit criteria:** every "TBD"/"open item" in the docs above has an owner
and an answer, or an explicit "defer to Phase N" note.

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

- [ ] Nano: add `INJ_TEST`/`COIL_TEST`/`TEST_STOP` command handling per
      [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md) §4.3, with hard-coded
      max pulse-width/dwell/duty-cycle caps enforced independent of any
      input.
- [ ] Nano: extend `safetyOK()`-style supervision to run continuously
      during a test, not just before it starts (mirrors existing relay
      re-check-after-energize pattern in `relayOnSafe()`).
- [ ] Nano: extend status reporting with `TEST_STATUS` lines.
- [ ] ESP32: add BLE peripheral (NimBLE), implement the GATT service from
      [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md) §1, gated by the chosen
      auth approach (§3).
- [ ] ESP32: add test-orchestration layer that validates app commands
      against the selected module profile's limits before forwarding to
      Nano.
- [ ] ESP32: add `/REPORTS` writer producing the result-record schema
      (SRS §6.2) on test completion.
- [ ] ESP32: extend module `.INI` schema support for `[TEST_INJECTOR]`/
      `[TEST_COIL]` sections (SRS §6.1); update
      `HILUX_1KD_TURBO.INI` and add at least a couple more example
      profiles.
- [ ] Coexistence testing: confirm Wi-Fi AP + WebServer + SD + NimBLE run
      concurrently without memory exhaustion/instability on target ESP32
      hardware; fall back per Phase 0 decision if not.
- [ ] Update `README.md`/`PROJECT_STATUS.md` once this phase lands (they
      currently describe pre-BLE, pre-driver-stage firmware).

**Exit criteria:** a BLE client (even a generic BLE test app, pre-mobile-
app) can pair with a PIN, request an injector pulse test, and receive
status/result notifications, with the Nano safety layer still the sole
authority over the driver outputs.

**Depends on:** Phase 1 (hardware to drive), Phase 0 (protocol/auth
decisions).

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
      resolved as a fuller test menu; module-selection screen and
      empty/error states beyond the basics **still not done** (see below).
- [ ] Local persistence for in-session results across app restarts (SRS
      NFR-REL-1) — currently session-only, not yet implemented.
- [ ] Module-profile selection screen — the app can send `SELECT_MODULE`
      but has no UI to pick from the SD card's module list yet.

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
- [ ] Security pass on BLE auth (rate-limiting, default-PIN warning,
      link-encryption decision from Phase 0/API spec §3).
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
- [ ] Revisit CAN/K-Line driver work (explicitly out of scope through
      Phase 4) once the core injector/coil product is validated — this is
      where `[COMMUNICATION]`/`[SERVICE]` sections of the module schema
      finally get consumed, under the same "validate before implementing
      OEM routines" caution already stated in the existing firmware
      headers.

**Depends on:** Phase 4.

---

## Dependency graph

```
Phase 0 (decisions/docs)
   │
   ├──▶ Phase 1 (driver hardware) ──▶ Phase 2 (firmware) ──▶ Phase 3 (mobile app) ──▶ Phase 4 (validation) ──▶ Phase 5 (field-ready)
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
| No reference design for 4 of the UI screens | Low/Medium — rework risk | 3 | Close UI_UX_SPEC §6 gaps in Phase 0 before building those screens |
| No version control on the repo today | Low but compounding | 0 | Initialize git immediately |

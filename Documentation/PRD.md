# Product Requirements Document (PRD)
## TIFF TESTER PRO — Automotive Injection & Ignition Diagnostic Tester

_Version: 0.2 (Draft) — 2026-09-29_
_Status: Planning — establishes target scope for full system build-out_

---

## 1. Purpose

Define the product this becomes once fully built: a bench-first, then
field-portable, automotive diagnostic tester that lets a technician test
injectors, ignition coils, and (later) other actuators/sensors on a bench —
safely, repeatably, and with a saved record per test — controlled from a
companion mobile app over Bluetooth Low Energy (BLE).

This PRD covers the **whole product**: hardware, firmware (ESP32 + Nano), and
the mobile app shown in the supplied screenshots ("TIFF TESTER PRO"). It is
the parent document for [SRS.md](SRS.md), [ARCHITECTURE.md](ARCHITECTURE.md),
[UI_UX_SPEC.md](UI_UX_SPEC.md), [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md),
and [ROADMAP.md](ROADMAP.md).

A separate, broader source document — "TIFF TESTER PRO app data.doc" (client
vision brief, 53 sections, not versioned in this repo) — describes a
full multi-brand vehicle diagnostic platform: CAN/K-Line diagnostics,
vehicle/module database CRUD, live data graphs, SD-logging UI, a
communication monitor, access levels, error history, and more. This PRD's
§7.1–7.3 scope is a deliberately narrowed **Phase 1–3 subset** of that vision
(the bench injector/coil tester shown in the supplied screenshots). §11
below reconciles the two: everything in the vision brief not already covered
by §7.1 is tracked as explicit backlog rather than silently dropped.

## 2. Background / current state

The existing repository ([PROJECT_STATUS.md](PROJECT_STATUS.md)) implements:
- An ESP32 main controller that hosts a Wi-Fi AP + minimal HTML web UI, an SD
  card for module-profile storage, and a UART link to a Nano.
- An Arduino Nano safety controller that independently supervises supply
  voltage, DUT voltage, current, e-stop, and ESP32 heartbeat, and is the only
  thing that ever energizes the DUT relay.
- One example module profile (`.INI`) for a Toyota Hilux 1KD-FTV turbo
  actuator, defining the profile schema.
- No CAN/K-Line driver code, no test-report persistence, no mobile app, no
  BLE.

The supplied screenshots show a **different, more advanced front end** than
the current web UI: a native/hybrid mobile app named "TIFF TESTER PRO" that
connects over **Bluetooth (BLE)** with **PIN-based pairing**, and drives
**injector and ignition-coil tests** with a defined test → progress → results
→ PDF report flow. This PRD treats the mobile app as the primary intended
product front end going forward, with the existing Wi-Fi web UI retained as a
bench/engineering fallback (see [ARCHITECTURE.md](ARCHITECTURE.md) §2 for how
the two coexist).

## 3. Problem statement

Independent technicians and small workshops testing injectors/coils off-car
today rely on ad-hoc bench rigs, multimeters, and manual procedures with no
standardized safety interlocks, no saved records, and no per-vehicle test
profiles. Mistakes (wrong pulse width, no current limiting, no e-stop) risk
damaging expensive components. TIFF TESTER PRO productizes a safe,
repeatable, per-module-profile bench test workflow with a mobile-first UI and
a paper trail (PDF report) per test session.

## 4. Goals

1. **Safety first, always.** DUT power is only ever live when an independent
   safety controller (Nano) has verified it is safe, and can be removed
   within one supervision cycle regardless of app/ESP32 state.
2. **Fast, guided bench workflow.** Connect → select module/injector/coil →
   run test → see pass/fail → save report, in as few taps as the screenshots
   show.
3. **Extensible module database.** New vehicle/module profiles can be added
   (initially by pasting/uploading `.INI` text, later via a curated library)
   without firmware changes.
4. **Trustworthy results.** Every test result is logged with a timestamp and
   can be exported as a PDF report tied to the module/vehicle being tested.
5. **Field-ready hardware.** The bench prototype path (USB power, breadboard)
   evolves into an enclosed, connectorized field unit.

## 5. Non-goals (for the scope this PRD covers)

- Performing OEM ECU flashing, security-gated UDS routines, or odometer/immobilizer
  work. `[SERVICE]` routines in module profiles remain explicitly gated and
  are out of scope until independently validated and legally reviewed per
  procedure.
- Cloud sync / multi-technician fleet management (may become a v2+ goal, not
  committed here).
- iOS app (Android is the primary target per the screenshots' Android-style
  UI chrome; iOS is a possible future port, not committed).
- Replacing a full OEM scan tool for general vehicle diagnostics (CAN/K-Line
  work here is scoped to the specific module tests defined in a profile, not
  general OBD-II diagnostics).

## 6. Target users / personas

| Persona | Description | Primary need |
|---|---|---|
| **Independent auto-electrical technician** | Runs a small workshop, tests injectors/coils pulled from customer vehicles before deciding to repair/replace | Fast pass/fail answer, safe bench test, printable proof for the customer |
| **Reman/rebuild shop QC tech** | Tests remanufactured injectors/coils in batches before resale | Repeatable test parameters per part number, saved results, throughput |
| **Mobile diagnostic technician (future)** | Uses a portable version in the field, on-vehicle-adjacent bench testing | Same UI, battery/portable power, ruggedized enclosure |
| **Firmware/hardware maintainer (internal)** | 2047 Tech Projects team member extending the platform | Clear module profile schema, wiring docs, safety architecture they can trust and extend |

## 7. Product scope — feature set

### 7.1 Mobile app (from screenshots — confirms this is in scope)
- Splash screen with branding, version.
- Home dashboard: connection status, ECU ON/OFF control, quick-test tiles
  (Injectors 1–4, Coils 1–4, All Injectors, System Info), bottom nav
  (Home / Tests / Results / Settings).
- BLE device scan screen with RSSI, device name/MAC.
- Pairing screen with PIN entry (default PIN shown, masked entry).
- Connected dashboard (post-pairing) mirroring home layout with live status.
- Per-injector test screen: injector select (1–4), pulse width (ms), test
  duration (s), start/stop, live status/last result/response.
- Per-coil test screen: coil select (1–4), dwell time (ms), test duration
  (s), start/stop, live status.
- "All injectors" sequential test screen: pulse width, per-injector test
  duration, progress bar (`x/4`), aggregate status.
- Test-in-progress state: elapsed time, live response (`OK`/fault),
  stop control.
- Results screen: chronological list of individual test results
  (pass/fail, timestamp), clear results, **Save PDF Report**.

### 7.2 Firmware / hardware (extends current repo)
- BLE peripheral role on ESP32 (GATT service for status, test control,
  pairing/PIN) — new, replacing/augmenting current REST-over-WiFi as the
  primary mobile transport.
- PIN-based pairing/bonding flow matching the app screens.
- Injector driver stage: 4-channel low-side (or high-side, TBD in hardware
  design) driver capable of controlled pulse-width injector firing, with
  current sensing per channel or shared.
- Ignition coil driver stage: 4-channel dwell-time-controlled coil driver.
- Nano safety envelope extended to supervise the new driver stages (not just
  the single DUT relay it supervises today).
- Module-profile-driven test parameter defaults (pulse width / dwell time /
  pass-fail thresholds sourced from the selected `.INI` profile, with the app
  allowing manual override as shown in the screenshots' editable fields).
- Test report persistence to SD (`/REPORTS`) in a format the app can also
  regenerate as PDF, and/or PDF generation on-app from BLE-received result
  data.
- Existing Wi-Fi AP + web UI retained as a bench/engineering interface (SD
  module management, raw status, firmware debug) — not removed.

### 7.3 Out of scope this phase (backlog)
- CAN/K-Line diagnostic engine and OEM `[SERVICE]` routine execution.
- Cloud/report sync, multi-user accounts.
- iOS app.
- Enclosure/production hardware (industrial design) beyond a functional
  enclosed prototype.
- Everything listed in §11 (full-system vision items not yet scoped into a
  phase) — CAN/K-Line status display and control, vehicle/module database
  CRUD, live data graphs, an SD-logging UI, relay/MOSFET-granular manual
  control, a diagnostics screen, a raw communication monitor, access levels,
  and error history.

## 8. Success metrics

| Metric | Target |
|---|---|
| Time from "app open" to first test result on a paired device | < 60 s |
| False DUT-energize (safety) incidents in bench testing | 0 |
| Nano-independent power cutoff latency (fault → relay off) | < 100 ms |
| Module profiles supported at v1 launch | ≥ 5 common injector/coil part families |
| Test report generation success rate | ≥ 99% of completed tests produce a saved report |
| BLE reconnect success after drop | ≥ 95% within 5 s, auto-retry |

## 9. Key risks / open questions

- **Transport decision (BLE vs Wi-Fi vs both):** screenshots show BLE only.
  Confirm whether Wi-Fi AP/web UI stays as a parallel interface or is
  deprecated in favor of BLE-only for the app, with Wi-Fi kept purely for
  bench/firmware debugging. *(Assumption used throughout these docs: both
  coexist — see [ARCHITECTURE.md](ARCHITECTURE.md).)*
- **Injector/coil driver hardware** is not yet designed — this is new
  hardware, not present in the current repo, and is the single biggest
  unscoped item (drive electronics, flyback protection, per-channel current
  sensing).
- **PDF generation location:** on-device (ESP32, resource constrained) vs.
  on-app (Android, easy). Recommendation: generate on-app from structured
  result data sent over BLE (see [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md)).
- **Default PIN security:** screenshots show a static default PIN
  (`TIFF2026`) — acceptable for bench use, needs a "change PIN" flow before
  this ships beyond internal use.
- **Regulatory/legal scope of `[SERVICE]` routines** (e.g. DPF forced
  regen, injector pilot learn) — remains explicitly gated pending review;
  not part of this phase's committed scope.

## 10. Dependencies

- Android target SDK/version range (needs a decision — see
  [SRS.md](SRS.md) §5).
- BLE stack choice on ESP32 (NimBLE recommended over the stock Bluedroid
  stack for memory headroom alongside Wi-Fi AP + SD + web server).
- Injector/coil driver hardware selection (part numbers, current ratings).
- PDF library choice for Android app.

## 11. Full-system vision alignment (2026-09-29 gap review)

A gap review compared the as-built app ([PROJECT_STATUS.md](PROJECT_STATUS.md))
against "TIFF TESTER PRO app data.doc" (the 53-section client vision brief
referenced in §1). Conclusion: **the app does not conform to that brief**,
by design — it implements a Phase 1–3 subset (§7.1) of a much larger target.
Nothing here was accidentally dropped; the items below were previously
implicit in "future features" language and are now tracked explicitly.

### 12.1 In scope and delivered (matches the vision brief)
- BLE connect/pair flow, live status receiving, PDF report export, offline
  operation, module-profile-driven test parameters — see §7.1 and
  [PROJECT_STATUS.md](PROJECT_STATUS.md).
- One transport difference from the brief: it specifies **Bluetooth
  Classic**; this product uses **BLE** (decided in
  [ROADMAP.md](ROADMAP.md) Phase 0) — an intentional, documented deviation,
  not a gap.

### 12.2 Vision-brief items not yet scoped into any phase (new backlog)
Tracked in [ROADMAP.md](ROADMAP.md) Phase 6 ("Full multi-brand diagnostic
platform"). Grouped by area, with the originating brief section in
parentheses:

- **Vehicle & module database CRUD** — ✅ delivered (2026-09-29): add/edit/
  delete/search/duplicate for vehicles and modules from within the app,
  fully offline (see [ROADMAP.md](ROADMAP.md) Phase 6). Export/import and
  the cascading Manufacturer → Model → Year → Engine → Module *selector
  inside the test flow* (as opposed to management screens) remain open
  (brief §13–§16).
- **CAN / K-Line diagnostics UI** — status display (online/offline, speed,
  IDs, protocol), plus a Diagnostics screen (READ STATUS / READ FAULTS /
  CLEAR FAULTS / RESET MODULE) (brief §6, §11, §51). Firmware drivers exist
  ([PROJECT_STATUS.md](PROJECT_STATUS.md)) but are hardware-unvalidated and
  have zero app-side UI.
- **Live data graphs** — real-time voltage/current/power/position/
  temperature charts with start/stop/clear (brief §10). No charting
  dependency or screen exists.
- **SD-logging UI** — dedicated screen: SD status, start/stop logging,
  view/export/delete log files (brief §6, §20–§21). SD reports are written
  firmware-side today; nothing surfaces them for browsing in-app.
- **Relay/MOSFET-granular manual control** — ✅ delivered (2026-09-29): new
  Controls screen with explicit "DUT Relay" labeling (replacing the
  previous ambiguous "ECU ON/OFF"), Reset Fault, disabled-with-explanation
  Auxiliary Relay/MOSFET placeholders pending Phase 1 hardware, and an
  always-visible, no-confirmation "ALL OUTPUTS OFF" distinct from normal
  DUT OFF (see [ROADMAP.md](ROADMAP.md) Phase 6; brief §17–§18, §37).
- **Communication Monitor** — raw TX/RX command log with clear/start/stop
  (brief §33). Does not exist; would sit alongside the existing BLE
  command/result streams.
- **Access levels** — Technician / Advanced-Engineer / Administrator modes
  gating raw CAN/K-Line/relay/MOSFET access (brief §36). Not implemented;
  no such distinction exists today.
- **Error History screen** — persisted fault log (date/time, type,
  voltage/current, module, action taken), independent of the Results screen
  (brief §42). Not implemented.
- **Step-by-step test procedure wizard** — Previous/Next/Start driven by a
  per-module procedure definition, and module-specific dynamic controls
  (position %, RPM, PWM %, open/close) instead of the current hardcoded
  pulse-width/duration sliders (brief §43–§44).
- **Richer connection-error handling** — reason-specific messaging (device
  off / Bluetooth disabled / lost / timeout) with Reconnect / Scan Again /
  Open Bluetooth Settings actions, vs. today's plain DISCONNECTED state
  (brief §34).
- **Results history persistence & global search** — Results currently
  persist across restarts ([ROADMAP.md](ROADMAP.md) Phase 3), but there is
  no search across vehicles/modules/results/faults/logs (brief §22, §26),
  and channel-test pass/fail can't yet show expected-vs-actual (brief §23)
  since it depends on Phase 1 driver hardware.
- **Technician notes field** — free-text observation per test result,
  saved with the record (brief §25). Not implemented.
- **Database backup/restore, module/vehicle export-import** — transfer a
  module/vehicle database between phones (brief §31). Not implemented.
- **About screen** and a full 13-item navigation surface (brief §4) — the
  app currently has a 4-tab shell (Home/Tests/Results/Settings); the brief's
  Dashboard/Connect/Diagnostics/Module Tester/Controls/Live Data/SD
  Logging/Modules/Vehicles/Test Results/Reports/Settings/About split is not
  built.
- **Delete confirmations / pre-delete backup** for destructive database
  operations (brief §46) — not applicable yet since there's no in-app
  database CRUD to protect.

None of the above blocks the current Phase 1–4 goal (a working bench
injector/coil tester). They become relevant once the product's ambition
grows beyond that bench tool into the multi-brand platform the vision brief
describes — see [ROADMAP.md](ROADMAP.md) Phase 6 for sequencing.

## 12. Related documents

- [SRS.md](SRS.md) — detailed functional/non-functional requirements
- [ARCHITECTURE.md](ARCHITECTURE.md) — system architecture & data flow
- [UI_UX_SPEC.md](UI_UX_SPEC.md) — full screen-by-screen UI spec
- [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md) — BLE/REST protocol contract
- [ROADMAP.md](ROADMAP.md) — phased delivery plan
- [PROJECT_STATUS.md](PROJECT_STATUS.md) — as-built status of current repo
- "TIFF TESTER PRO app data.doc" — client's full-system vision brief (53
  sections); not stored in this repo. See §11 for the reconciliation against
  current scope.

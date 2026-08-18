# Software Requirements Specification (SRS)
## TIFF TESTER PRO — Firmware & Mobile App

_Version: 0.1 (Draft) — 2026-08-18_
_Structure loosely follows IEEE 830._
_Parent document: [PRD.md](PRD.md)_

---

## 1. Introduction

### 1.1 Purpose
Specifies functional and non-functional requirements for the three software
components of TIFF TESTER PRO: the **ESP32 firmware**, the **Arduino Nano
safety firmware**, and the **Android mobile app**.

### 1.2 Scope
Covers requirements needed to deliver the feature set in
[PRD.md](PRD.md) §7: BLE-paired mobile control of injector/coil bench
testing, module-profile-driven test parameters, independent hardware safety
supervision, and saved test reports.

### 1.3 Definitions

| Term | Meaning |
|---|---|
| DUT | Device Under Test (injector, coil, actuator) |
| Module profile | `.INI` file describing a specific vehicle/module's test parameters and limits |
| Nano | Arduino Nano safety/I-O controller |
| GATT | BLE Generic Attribute Profile |
| Dwell time | Time current is allowed to build in an ignition coil primary before spark |
| Pulse width | Time an injector driver holds an injector open |
| E-stop | Emergency stop |

### 1.4 References
[PRD.md](PRD.md), [ARCHITECTURE.md](ARCHITECTURE.md),
[UI_UX_SPEC.md](UI_UX_SPEC.md), [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md),
[PINOUT_AND_WIRING.txt](PINOUT_AND_WIRING.txt), existing firmware in
`ESP32_Firmware/` and `Arduino_Nano_Safety/`.

---

## 2. Overall description

### 2.1 System context
Three cooperating components:
1. **Mobile app (Android)** — user-facing control and results.
2. **ESP32** — BLE peripheral + Wi-Fi AP/web server + SD storage + CAN/K-Line
   (future) + UART bridge to Nano + injector/coil driver command layer.
3. **Nano** — independent safety supervisor and the sole authority that
   energizes any DUT-facing power/driver output.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full diagram.

### 2.2 Assumptions and constraints
- ESP32 board has sufficient flash/RAM to run Wi-Fi AP + BLE (NimBLE) + SD +
  web server concurrently; if not, Wi-Fi AP may need to become on-demand
  rather than always-on (open question, tracked in ROADMAP).
- Nano remains AVR (ATmega328P-class); no BLE/Wi-Fi stack runs on it by
  design — it must stay simple enough to be trustworthy as the safety layer.
- Injector/coil driver hardware is new and unspecified at time of writing;
  requirements below define behavior, not final circuit values.
- Android only for v1 app (see PRD §5 non-goals).

---

## 3. Functional requirements

Requirements are numbered `FR-<area>-<n>` and tagged with priority:
**M**ust, **S**hould, **C**ould (MoSCoW).

### 3.1 Connectivity & pairing
- **FR-CONN-1 (M):** ESP32 shall advertise as a BLE peripheral with a
  discoverable name (e.g. `TiffTester`) including in advertisement data a
  way to distinguish multiple units (e.g. suffix from MAC).
- **FR-CONN-2 (M):** App shall scan for BLE devices and list name, MAC
  address, and RSSI, refreshing live (matches screenshot 3).
- **FR-CONN-3 (M):** Connection shall require PIN entry; ESP32 shall reject
  GATT writes to control characteristics from an unauthenticated connection.
- **FR-CONN-4 (S):** PIN shall be user-changeable from the app; a factory
  default PIN shall be documented and changeable on first connect.
- **FR-CONN-5 (M):** App shall show connection status (Connected/
  Disconnected) and device identity persistently on the dashboard.
- **FR-CONN-6 (S):** On unexpected BLE disconnect, app shall attempt
  automatic reconnect for a bounded retry window and surface the state to
  the user.
- **FR-CONN-7 (M):** Losing the BLE link shall never be the sole event that
  leaves DUT power energized — Nano heartbeat timeout (independent of BLE)
  is the authoritative cutoff (see FR-SAFE-*).

### 3.2 ECU / DUT power control
- **FR-PWR-1 (M):** App shall provide explicit ECU/DUT ON and OFF controls
  (matches screenshots 2/5).
- **FR-PWR-2 (M):** DUT power shall default OFF on every boot and on every
  new BLE connection (no implicit "on" state carried over).
- **FR-PWR-3 (M):** ECU ON request shall only result in power if Nano
  safety checks pass; a rejected request shall return a specific reason to
  the app (voltage/current/e-stop/etc.).

### 3.3 Module profile management
- **FR-MOD-1 (M):** ESP32 shall continue to store module profiles as `.INI`
  files on SD under `/MODULES` and validate on upload (existing behavior,
  retained).
- **FR-MOD-2 (S):** App shall be able to list and select an available module
  profile from the ESP32 (via BLE or Wi-Fi) rather than requiring the
  technician to know test parameters manually.
- **FR-MOD-3 (S):** Selecting a module profile shall pre-fill test screens'
  default parameters (pulse width, dwell time, min/max thresholds) from that
  profile's `[TEST_*]`/`[SAFETY]` sections; the technician may still edit
  values per-run (matches editable fields in screenshots 6/7/8).
- **FR-MOD-4 (C):** App shall support uploading/pasting a new module profile
  (parity with the existing web UI's `/api/module`).

### 3.4 Injector test
- **FR-INJ-1 (M):** App shall let the technician select injector 1–4,
  individually (screenshot 6).
- **FR-INJ-2 (M):** App shall let the technician set pulse width (ms) and
  test duration (s) before starting.
- **FR-INJ-3 (M):** Firmware shall drive the selected injector channel for
  the requested pulse width, repeated as needed to fill the requested test
  duration, and report per-pulse or aggregate current/response telemetry.
- **FR-INJ-4 (M):** Firmware shall enforce a hard maximum pulse width and
  duty cycle independent of app input, to protect injector coils from
  thermal damage regardless of a bad/malicious app value.
- **FR-INJ-5 (M):** App shall show live status (`IDLE`/`TESTING`/`DONE`/
  `FAULT`), last result, and raw response text (matches screenshot 6/9).
- **FR-INJ-6 (M):** Test result (pass/fail + reason) shall be appended to
  the session's results list with a timestamp.

### 3.5 Ignition coil test
- **FR-COIL-1 (M):** App shall let the technician select coil 1–4
  individually (screenshot 7).
- **FR-COIL-2 (M):** App shall let the technician set dwell time (ms) and
  test duration (s).
- **FR-COIL-3 (M):** Firmware shall drive the selected coil channel for the
  configured dwell time per cycle, repeated for the test duration, and
  report spark/current response telemetry if the driver hardware supports
  sensing it.
- **FR-COIL-4 (M):** Firmware shall enforce a hard maximum dwell
  time/duty independent of app input (coil primary current/heat
  protection).
- **FR-COIL-5 (M):** Same status/result/telemetry requirements as
  FR-INJ-5/6, coil-specific.

### 3.6 All-injectors sequential test
- **FR-ALL-1 (M):** App shall provide a single control to sequentially test
  all 4 injectors with shared pulse width and per-injector test duration
  (screenshot 8).
- **FR-ALL-2 (M):** Firmware/app shall report progress as `x/4` and overall
  status.
- **FR-ALL-3 (S):** A stop control shall abort the sequence after the
  current injector's cycle completes (not mid-pulse).

### 3.7 Results & reporting
- **FR-RES-1 (M):** App shall maintain a chronological results list for the
  current session: item, pass/fail, timestamp (screenshot 10).
- **FR-RES-2 (M):** App shall provide "Clear Results" for the current
  session list.
- **FR-RES-3 (M):** App shall provide "Save PDF Report" that generates a
  PDF containing: unit/app version, module/vehicle identity (if selected),
  each test's parameters, result, and timestamp, and an overall summary.
- **FR-RES-4 (S):** ESP32 shall also persist a machine-readable copy of each
  completed test session to SD under `/REPORTS` (independent of whether the
  app-side PDF succeeds), so bench records survive app data loss.
- **FR-RES-5 (C):** App shall allow sharing/exporting the saved PDF via
  standard Android share sheet.

### 3.8 System info / settings
- **FR-SYS-1 (S):** App shall provide a "System Info" view showing firmware
  version (ESP32 + Nano), connected device identity, SD status, and battery/
  supply voltage.
- **FR-SYS-2 (C):** Settings screen shall allow changing the pairing PIN,
  toggling units, and clearing local app data.

### 3.9 Safety (mirrors and extends existing Nano behavior)
- **FR-SAFE-1 (M):** Nano shall keep all DUT-facing outputs (relay,
  injector drivers, coil drivers) OFF at boot and OFF whenever heartbeat
  from ESP32 is stale beyond the configured timeout — unchanged from
  current firmware, extended to cover the new driver channels.
- **FR-SAFE-2 (M):** Nano shall independently monitor supply voltage, DUT
  voltage, and current, and cut all DUT-facing outputs if any configured
  limit is exceeded, regardless of what the ESP32 or app requested.
- **FR-SAFE-3 (M):** E-stop input shall immediately cut all DUT-facing
  outputs and require an explicit `RESET_FAULT` before any output can be
  re-armed.
- **FR-SAFE-4 (M):** Nano hardware watchdog shall remain enabled; a Nano
  firmware lockup shall result in a reset with all outputs defaulting OFF.
- **FR-SAFE-5 (M):** BLE/app disconnects or app crashes shall not by
  themselves be treated as unsafe (Wi-Fi/BLE is a UI layer, not the safety
  layer) — but the ESP32-to-Nano heartbeat, which the ESP32 only sends while
  it itself considers the app session live/sane, is the enforced mechanism
  (i.e. ESP32 shall stop sending heartbeats if it loses its BLE central
  connection during an active test, so the existing heartbeat-timeout path
  in Nano cuts power without any Nano changes).

---

## 4. External interface requirements

### 4.1 Mobile app UI
Full detail in [UI_UX_SPEC.md](UI_UX_SPEC.md); this SRS only requires that
implementation matches the ten reference screens supplied (splash, home,
scan, pairing, connected dashboard, injector test, coil test, all-injectors
test, test-in-progress, results).

### 4.2 BLE interface
Full detail in [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md). Summary
requirement: a custom GATT service exposing status (notify), command
(write), and result (notify) characteristics, PIN-gated.

### 4.3 Existing Wi-Fi/REST interface
Retained as-is for bench/engineering use (`/`, `/api/status`,
`/api/power/on|off`, `/api/module`, `/api/modules`); not user-facing in the
mobile app for v1.

### 4.4 UART (ESP32 ↔ Nano)
Retained line-based protocol (`HEARTBEAT`, `POWER_ON`, `POWER_OFF`,
`STATUS`, `RESET_FAULT`), extended with new commands for injector/coil
channel control — see [API_PROTOCOL_SPEC.md](API_PROTOCOL_SPEC.md) §4.

### 4.5 SD card
Existing `/MODULES`, `/REPORTS`, `/LOGS` structure retained; `/REPORTS`
gains actual writers per FR-RES-4.

---

## 5. Non-functional requirements

| ID | Requirement |
|---|---|
| NFR-PERF-1 | BLE status updates shall reach the app within 250 ms of a state change (matches existing 250 ms heartbeat cadence). |
| NFR-PERF-2 | App shall render test-in-progress elapsed time with ≤1 s drift over a 60 s test. |
| NFR-SAFE-1 | Nano safety-loop cycle time shall remain ≤ current implementation's effective loop rate; any change must not increase fault-to-cutoff latency beyond 100 ms. |
| NFR-REL-1 | App shall handle BLE disconnect/reconnect without losing an in-progress results list. |
| NFR-REL-2 | ESP32 shall continue operating its Nano-facing heartbeat/status loop even if Wi-Fi and BLE are both idle/disconnected (safety supervision is not contingent on any radio link). |
| NFR-USE-1 | Core flow (connect → run one test → view result) shall be achievable by a first-time user without external instructions, consistent with the screenshots' guided single-purpose screens. |
| NFR-SEC-1 | BLE pairing shall not accept control writes pre-authentication; PIN shall not be transmitted or logged in plaintext outside the pairing exchange itself. |
| NFR-SEC-2 | Default PIN shall be changeable and the app shall warn if a default/factory PIN is still active. |
| NFR-COMPAT-1 | Android app shall target a documented minimum SDK (recommend API 26/Android 8.0+ for broad BLE-permission-model compatibility) — to be confirmed in ROADMAP Phase 2. |
| NFR-MAINT-1 | Module profile schema changes shall remain backward compatible with the existing `.INI` format demonstrated in `HILUX_1KD_TURBO.INI`, or ship a migration note. |
| NFR-PORT-1 | Firmware additions shall not require replacing the ESP32 or Nano MCU already specified in [PINOUT_AND_WIRING.txt](PINOUT_AND_WIRING.txt), only additional driver-stage hardware. |

---

## 6. Data requirements

### 6.1 Module profile (existing schema, unchanged sections referenced)
`[MODULE]`, `[SAFETY]`, `[TEST_RESISTANCE]`, `[TEST_POSITION]`,
`[COMMUNICATION]`, `[TESTS]`, `[SERVICE]` — see
`SD_MODULES/MODULES/TOYOTA/HILUX_1KD_TURBO.INI` for the reference instance.
New optional sections proposed for injector/coil defaults:

```ini
[TEST_INJECTOR]
enabled=1
default_pulse_width_ms=3.0
max_pulse_width_ms=8.0
default_test_duration_s=5

[TEST_COIL]
enabled=1
default_dwell_ms=3.0
max_dwell_ms=8.0
default_test_duration_s=5
```

### 6.2 Test result record (new)
One record per individual test run, produced by ESP32 and mirrored by the
app:

```json
{
  "session_id": "string",
  "module_id": "TOYOTA_HILUX_1KD_TURBO",
  "test_type": "injector | coil | all_injectors",
  "channel": 1,
  "params": { "pulse_width_ms": 3.0, "duration_s": 5 },
  "result": "PASS | FAIL",
  "response": "string",
  "timestamp": "ISO-8601"
}
```

### 6.3 PDF report
Generated app-side from an ordered list of §6.2 records plus a header
(unit ID, firmware versions, module identity, date/time, technician note
field — optional future field).

---

## 7. Traceability (requirements ↔ screenshots)

| Screenshot | Requirements covered |
|---|---|
| 1. Splash | — (branding only) |
| 2. Home/Dashboard | FR-CONN-5, FR-PWR-1 |
| 3. Scan devices | FR-CONN-2 |
| 4. Pairing (PIN) | FR-CONN-3, FR-CONN-4, NFR-SEC-1/2 |
| 5. Connected dashboard | FR-CONN-5, FR-PWR-1/2/3 |
| 6. Injector test | FR-INJ-1..6, FR-MOD-3 |
| 7. Coil test | FR-COIL-1..5, FR-MOD-3 |
| 8. All injectors test | FR-ALL-1..3 |
| 9. Test in progress | FR-INJ-5, NFR-PERF-2 |
| 10. Results | FR-RES-1..5 |

---

## 8. Open items requiring a decision before build

1. Injector/coil driver circuit topology and per-channel current-sense
   method (blocks FR-INJ-3, FR-COIL-3 implementation detail).
2. Whether Wi-Fi AP stays always-on alongside BLE, or becomes on-demand
   (resource/coexistence risk — NFR-REL-2, PRD §9).
3. Android minimum SDK / target device baseline.
4. PDF library selection for Android.
5. Whether module-profile selection happens over BLE (bandwidth-constrained)
   or the app falls back to Wi-Fi for that specific transfer.

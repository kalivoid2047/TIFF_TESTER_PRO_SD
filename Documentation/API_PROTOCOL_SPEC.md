# API & Protocol Specification
## TIFF TESTER PRO — BLE, Wi-Fi/REST, and UART contracts

_Version: 0.2 (BLE service + app implemented) — 2026-08-18_
_Parent document: [PRD.md](PRD.md) · Requirements: [SRS.md](SRS.md) ·
Architecture: [ARCHITECTURE.md](ARCHITECTURE.md)_

This document was originally a proposed contract; §1 now reflects what is
actually implemented in `ESP32_Firmware/ble_service.ino` and
`Mobile_App/`. The UUIDs are real and in use. The Command characteristic
ended up as plain colon/comma-delimited text rather than JSON (§1.3) —
simpler to parse on both an Arduino `String` and Dart, and small enough
that MTU was never a concern. The Wi-Fi/REST (§2) and UART (§4) sections
were also implemented essentially as originally proposed.

---

## 1. BLE GATT service (new)

### 1.1 Service
- **Service UUID:** `TBD — mint a 128-bit vendor UUID`, e.g.
  `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`-style custom UUID (placeholder,
  must be generated fresh, not reused from Nordic UART or other examples in
  production).
- **Advertised name:** `TiffTester` (+ optional short suffix from MAC for
  multi-unit disambiguation, e.g. `TiffTester-71:13`).

### 1.2 Characteristics

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| Auth/PIN | `...0002` | Write | App writes PIN (or a challenge-response value, see §3) to authenticate the connection before any other characteristic accepts writes. |
| Status | `...0003` | Read, Notify | Mirrors the existing `NanoStatus` struct: relay/driver state, fault, e-stop, watchdog, supply/DUT voltage, current, fault text. Notifies at ~250 ms cadence (matches current heartbeat/status interval). |
| Command | `...0004` | Write | App → ESP32 control messages: power on/off, run test, stop test, select module. See §1.3. |
| Test Result | `...0005` | Notify | ESP32 → app: completed test result records (SRS §6.2 JSON, or a compact binary encoding if BLE MTU is a constraint). |
| Module List | `...0006` | Read | List of module IDs available on SD (for FR-MOD-2). |
| Device Info | `...0007` | Read | Firmware versions (ESP32 + Nano), unit serial/ID. |

### 1.3 Command message format (Command characteristic) — as implemented

Plain text, written to the Command characteristic as a UTF-8 string (no
JSON). This is what `ble_service.ino`'s `BleCommandCallbacks` and the
Flutter app's `TiffBleService.sendCommand()` actually speak:

```
POWER_ON
POWER_OFF
RESET_FAULT
SELECT_MODULE:TOYOTA_HILUX_1KD_TURBO
RUN_TEST:resistance
RUN_TEST:short_to_ground
RUN_TEST:current_monitor
RUN_INJECTOR_TEST:<channel>,<pulse_width_ms>,<duration_s>
RUN_COIL_TEST:<channel>,<dwell_ms>,<duration_s>
RUN_ALL_INJECTORS:<pulse_width_ms>,<duration_per_s>
STOP_TEST
SET_PIN:<new_pin>
```

`SET_PIN` relies on the connection already being authenticated with the
*current* PIN (per §3) to reach the command handler at all, so it does not
separately re-check a "current PIN" the way the Wi-Fi `/api/config/pin`
endpoint does (that endpoint has no prior auth step to lean on). New PIN
must be at least 6 characters, same rule as the web API.

Every command (except `POWER_OFF`/`STOP_TEST`, which are always accepted
as safety-favorable) is silently ignored by the ESP32 if the connection
has not completed §3's PIN authentication.

**Important — hardware-honesty note:** `RUN_INJECTOR_TEST`,
`RUN_COIL_TEST`, and `RUN_ALL_INJECTORS` are accepted and always answered
on the Result characteristic, but currently always return
`NOT_IMPLEMENTED` — there is no injector/coil driver hardware on the
board yet (see [ROADMAP.md](ROADMAP.md) Phase 1). Only `RUN_TEST:resistance`
/ `short_to_ground` / `current_monitor` produce a real PASS/FAIL today,
computed from the Nano's existing voltage/current sensors. The app's
Injector/Coil/All-Injectors test screens work end-to-end today, they just
honestly report "not implemented" instead of either hanging or faking a
result — swapping in real pulse-driven tests later requires only a
firmware change, not an app change, since the command/result contract is
already what the app speaks.

`STOP_TEST` is currently a no-op on the firmware side: every test above
runs and completes synchronously (one sensor read, or an immediate
`NOT_IMPLEMENTED`), so there's nothing in-flight to interrupt yet. It's
accepted now so the app's Stop button has something valid to send once
real, longer-running pulse tests exist.

### 1.4 Status notify format
Reuses the existing `STATUS,relay,fault,estop,watchdog,supply,dut,current,
faulttext` CSV line already produced by the Nano — the ESP32 simply relays
it (as text or lightly re-encoded JSON) over the Status characteristic
instead of only over the Wi-Fi `/api/status` endpoint as it does today.
During an active test, an additional `test_state` field should be included:

```json
{
  "relay": 1, "fault": 0, "estop": 0, "watchdog": 1,
  "supply_v": 13.8, "dut_v": 0.0, "current_a": 0.02,
  "fault_text": "",
  "test_state": { "type": "injector", "channel": 1, "status": "TESTING", "elapsed_s": 3 }
}
```

## 2. Wi-Fi / REST interface (existing, retained)

No changes required to the existing endpoints for this phase; they remain
the bench/engineering interface (see [ARCHITECTURE.md](ARCHITECTURE.md) §2):

| Method | Path | Purpose |
|---|---|---|
| GET | `/` | HTML dashboard |
| GET | `/api/status` | Plain-text Nano status |
| GET | `/api/power/on` | Request DUT power on |
| GET | `/api/power/off` | Request DUT power off |
| POST | `/api/module` | Validate + save a module `.INI` |
| GET | `/api/modules` | List SD module files |

**Proposed additions** (optional, Should-have, useful for parity/debugging
without requiring the mobile app):
- `POST /api/test/injector` — body `{channel, pulse_width_ms, duration_s}`.
- `POST /api/test/coil` — body `{channel, dwell_ms, duration_s}`.
- `GET /api/reports` — list saved report files on SD.
- `GET /api/reports/{id}` — fetch one report's JSON.

## 3. Authentication (BLE PIN pairing)

The screenshots show an app-layer PIN (default `TIFF2026`), distinct from
BLE's own native pairing/bonding. Two implementation options, to be decided
in Phase 2:

**Option A — App-layer PIN only (simpler, matches screenshots literally).**
BLE connection itself is open (no OS-level bonding prompt); the app writes
the PIN to the Auth characteristic immediately after connecting; ESP32
compares against a stored PIN (default, then user-changed) and only then
un-gates the Command characteristic. Simpler to implement and matches the
UI exactly, but the PIN travels in what is still (absent extra work)
plaintext-over-BLE-link-layer-encryption territory unless BLE link
encryption is separately enabled.

**Option B — BLE LE Secure Connections bonding + app-layer PIN as a second
factor.** Uses the platform's standard BLE "Just Works"/passkey bonding in
addition to the app PIN. Stronger, but adds OS-level pairing UI that isn't
shown in the reference screenshots (the screens present the PIN entry as
happening *inside* the app, not as an OS dialog).

**Recommendation:** ship Option A for v1 (matches the specified UX exactly,
sufficient for a bench tool with a short-range trusted-environment threat
model), but enable BLE link-layer encryption (not just app-layer PIN
checking) so the PIN and command traffic aren't sent as plaintext over the
air, and document this as a defer-not-drop item — revisit before any
field/production deployment beyond internal bench use. Track as an explicit
decision in ROADMAP Phase 2.

Rate-limiting: ESP32 should lock out further PIN attempts with an
increasing backoff after e.g. 5 consecutive failures, to blunt brute-force
guessing of a 8-character PIN over BLE.

## 4. UART protocol (ESP32 ↔ Nano) — existing + proposed extension

### 4.1 Existing commands (unchanged)
```
HEARTBEAT
POWER_ON
POWER_OFF
STATUS
RESET_FAULT
```
### 4.2 Existing status line (unchanged)
```
STATUS,relay,fault,estop,watchdog,supply,dut,current,faulttext
```

### 4.3 Proposed new commands (injector/coil test support)
```
INJ_TEST,<channel>,<pulse_width_ms>,<duration_s>
COIL_TEST,<channel>,<dwell_ms>,<duration_s>
TEST_STOP
```
Nano is responsible for:
- Rejecting any `pulse_width_ms`/`dwell_ms` above its own hard-coded max
  (SRS FR-INJ-4/FR-COIL-4), regardless of what value was sent.
- Running the requested cycles only while `safetyOK()` (existing function)
  continues to pass.
- Reporting a new status line during/after a test:
```
TEST_STATUS,<type>,<channel>,<status>,<elapsed_s>,<last_current_a>,<result>,<faulttext>
```
  e.g. `TEST_STATUS,INJ,1,TESTING,3,1.85,,` while running, and
  `TEST_STATUS,INJ,1,DONE,5,1.90,PASS,` on completion.

### 4.4 Framing/robustness notes (apply to both existing and new lines)
- Existing line-based, newline-terminated, comma-separated format is
  retained for consistency and to minimize churn in the already-working
  Nano parser.
- ESP32's existing 200-char line buffer (`serialLine`) should be reviewed
  once `TEST_STATUS` lines are added, since they carry more fields than
  `STATUS` — confirm the new max line length stays under that buffer, or
  raise the buffer size.

## 5. Result record & report format

See [SRS.md](SRS.md) §6.2 for the canonical JSON result-record shape and
§6.3 for PDF contents. The ESP32's SD-side `/REPORTS` writer and the app's
BLE-received result stream should serialize to the *same* schema so a
report reconstructed from SD data (e.g. via the Wi-Fi debug endpoint) is
identical in structure to one built live from BLE notifications.

## 6. Versioning

Recommend adding a simple protocol version field to the BLE Device Info
characteristic and to the UART `STATUS`/`TEST_STATUS` lines once this
extension ships (e.g. `PROTO_VER=2`), so the app can detect and warn on a
version mismatch against older firmware in the field, rather than failing
silently on unrecognized fields.

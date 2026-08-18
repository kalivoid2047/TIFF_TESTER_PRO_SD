# UI/UX Specification
## TIFF TESTER PRO — Android App

_Version: 0.1 (Draft) — 2026-08-18_
_Source: 10 reference screenshots supplied by the user (splash → results flow)._
_Parent document: [PRD.md](PRD.md) · Requirements: [SRS.md](SRS.md)_

---

## 1. Visual style guide (derived from screenshots)

| Token | Value | Notes |
|---|---|---|
| Background | Near-black, `#0A0A0A`–`#121212` | Dark theme throughout, all screens |
| Primary accent / brand red | `#E31E24`-ish red | Logo, primary buttons (ECU OFF, Start Test, Stop, Save Report), active tab indicator, "TESTER" wordmark |
| Secondary accent / success green | Material-style green | ECU ON button, PASS status, "OK" response, connected status text |
| Warning/fail | Red (same family as brand red) | FAIL status, disconnected status |
| Purple accent | Medium purple | "All Injectors" tile and its primary action — used to visually separate the batch action from single-channel actions |
| Card surface | Slightly lighter than background, `#1A1A1A`–`#1E1E1E`, subtle border | Used for grouped content: Connection card, ECU Control card, Quick Tests grid, test-parameter panels |
| Text primary | White/near-white | Headings, values |
| Text secondary | Mid-gray | Labels ("Status", "Device", field labels) |
| Corner radius | Medium (~8–12dp) | Cards and buttons both rounded |
| Iconography | Simple line/glyph icons per tile (bluetooth, lightning/injector, coil, person-group for "all", info-circle for system info) | Matches bottom-nav icon set (home, tests, results, settings) |
| Typography | Sans-serif, bold for headings and button labels, regular for body/labels | Header uses two-tone: "TIFF" white, "TESTER" red, "PRO" white |

This is a **dark-mode-only** app per the screenshots; no light theme is shown
or should be assumed unless requested later.

## 2. Screen inventory

| # | Screen | Primary purpose |
|---|---|---|
| 1 | Splash | Branding, boot/init |
| 2 | Home / Dashboard (disconnected) | Entry point, connection status, quick tests |
| 3 | Scan Devices | BLE discovery |
| 4 | Connect to Device (PIN) | Pairing/authentication |
| 5 | Connected Dashboard | Same layout as #2, live/connected state |
| 6 | Injector Test | Single-injector test configuration & run |
| 7 | Ignition Coil Test | Single-coil test configuration & run |
| 8 | All Injectors Test | Sequential batch test |
| 9 | Injector Test — in progress | Live run state (same screen as #6, running state) |
| 10 | Test Results | Session result log, report export |

## 3. Navigation flow

```
Splash
  └─▶ Home (disconnected)
        ├─▶ Scan Devices ──▶ Connect (PIN) ──▶ Home (connected)
        └─▶ [bottom nav] Tests / Results / Settings (available, deeper detail TBD)

Home (connected)
  ├─▶ ECU ON / ECU OFF (in-place action, no navigation)
  ├─▶ Injectors 1–4 tile ──▶ Injector Test screen
  ├─▶ Coils 1–4 tile ──▶ Ignition Coil Test screen
  ├─▶ All Injectors tile ──▶ All Injectors Test screen
  └─▶ System Info tile ──▶ System Info screen (not in reference shots — spec in §4.11)

Injector Test / Coil Test / All Injectors Test
  └─▶ Start Test ──▶ (same screen, running state) ──▶ completion ──▶ Results screen (via bottom nav or auto-prompt, TBD)

Results
  ├─▶ Clear Results (in-place)
  └─▶ Save PDF Report (in-place, triggers Android share/save)
```

Bottom navigation bar is present on all main screens (Home, Tests, Results,
Settings) — 4 tabs, with a `←` back arrow used instead of bottom nav on
modal/flow screens (Scan, Connect, individual test screens, Results-as-drill-
down). Treat Home/Tests/Results/Settings as top-level tabs and
Scan/Connect/Test-detail as pushed screens reachable from Home or Tests.

## 4. Screen-by-screen specification

### 4.1 Splash Screen
- Full-bleed black background.
- Centered car/gear logo mark (red outline icon).
- Wordmark: "TIFF" (white) + "TESTER" (red) stacked over "PRO" (white),
  bold, large.
- Subtitle: "TOTAL INJECTION & IGNITION DIAGNOSTIC TESTER" (small,
  letter-spaced, gray).
- Loading spinner (small, red) + "Initializing..." text.
- Version string, e.g. "Version 1.0.0", bottom of screen.
- **Behavior:** auto-advances to Home after init (BLE stack ready, local
  storage/settings loaded); no user interaction.
- **States:** none beyond loading → navigate.

### 4.2 Home / Dashboard (disconnected)
Top app bar: hamburger/menu icon (left), "TIFF TESTER PRO" wordmark
(center-left, two-tone as above), Bluetooth icon (right, indicates BLE
subsystem state).

**Connection card**
- Label "CONNECTION" (small caps header).
- Row: "Status" → value "DISCONNECTED" (red, bold, right-aligned).
- Row: "Device" → value "Not Connected" (gray, right-aligned).
- Full-width red button: **SCAN FOR DEVICES** → navigates to Scan screen.

**ECU Control card**
- Label "ECU CONTROL".
- Two side-by-side buttons: **ECU ON** (green) / **ECU OFF** (red), equal
  width.
- **State when disconnected:** both buttons should be disabled/inert (spec
  gap in screenshots — recommend visually dimming and blocking taps with a
  toast "Connect to a device first" per FR-PWR-2/3 in SRS) — flag this as an
  implementation decision, not contradicted by the screenshots since #2 is
  shown pre-connection with the card visible but not demonstrated as tapped.

**Quick Tests card**
- Label "QUICK TESTS".
- 2×2 grid of tiles, each with icon + label + sublabel:
  - **Injectors** / "1–4" (blue lightning-bolt icon)
  - **Coils** / "1–4" (yellow/gold coil icon)
  - **All Injectors** / "Test all" (purple people/group icon)
  - **System Info** / "View info" (teal info icon)
- Tapping Injectors/Coils/All Injectors when disconnected: same disabled/
  toast pattern as ECU controls. System Info may remain available offline
  (shows cached firmware/app version).

Bottom nav: Home (active/red), Tests, Results, Settings — icons + labels.

### 4.3 Scan Devices
Top bar: `←` back arrow, title "SCAN DEVICES".
- Centered status row: Bluetooth icon + "Scanning for BLE devices..." +
  spinner, while actively scanning.
- Scrollable list of discovered devices, each row:
  - Device name (bold, white) — e.g. "TiffTester", "ESP32_Tester",
    "CAR_DIAG_01", "OBD_II"
  - MAC address (gray, monospace-ish, smaller) below the name
  - Right side: RSSI value (e.g. "RSSI -45") + a signal-strength bar icon,
    color/height reflecting strength
- Full-width red button pinned at bottom: **STOP SCAN** (toggles to
  "SCAN FOR DEVICES" when stopped, mirroring Home's button).
- **Behavior:** tapping a device row navigates to the Connect screen with
  that device pre-selected. List updates live as new devices/RSSI arrive;
  should only list devices advertising the TIFF Tester BLE service UUID in
  production (screenshot shows unrelated-looking entries like "OBD_II",
  "CAR_DIAG_01" — treat that as illustrative of a generic scan result set;
  real implementation should filter to compatible devices, or clearly badge
  non-TIFF devices as "unsupported").
- **Empty state (not shown, must design):** "No devices found" + retry.
- **Permission state (not shown, must design):** Android 12+ requires
  runtime BLUETOOTH_SCAN/CONNECT permission prompts before this screen can
  function — see SRS NFR-COMPAT-1.

### 4.4 Connect to Device (PIN)
Top bar: `←` back arrow, title "CONNECT TO DEVICE".
- Centered large circular icon with a padlock — red circle, white lock
  glyph.
- Prompt text: "Enter PIN to connect".
- Helper text: "Default PIN: TIFF2026" (red, smaller) — indicates a factory
  default is in use; per SRS NFR-SEC-2 the app should also warn/encourage
  changing it, ideally right after first successful connect.
- PIN input field: placeholder "Enter PIN", with a trailing eye icon to
  toggle masked/visible text.
- Full-width red button: **CONNECT**.
- Secondary, no-fill/outline-style button: **CANCEL** → back to Scan (or
  Home).
- **States:**
  - Default/empty — Connect button may be disabled until input non-empty.
  - Incorrect PIN — inline error under the field (not shown, must design):
    "Incorrect PIN. Try again."; do not lock out silently — follow standard
    rate-limiting practice (e.g. backoff after repeated failures) since this
    gates a safety-relevant control link.
  - Connecting — Connect button shows spinner/disabled while BLE
    pairing/auth completes.

### 4.5 Connected Dashboard
Same layout as Home (§4.2) with connected-state values:
- Status → "CONNECTED" (green, bold).
- Device → device name + MAC (e.g. "TiffTester" / "00:1A:7D:0A:71:13"),
  plus a signal bar icon.
- Top-right icon changes from plain Bluetooth to a Bluetooth-with-status
  variant (screenshot shows a small circular refresh/settings icon here
  too — treat as a quick-settings/refresh affordance, exact action TBD).
- ECU ON / ECU OFF buttons now active; tapping either sends the
  corresponding command (SRS FR-PWR-1) and the app should reflect the
  resulting state (e.g. button emphasis or a status chip) even though the
  reference shot doesn't show a third "ECU is currently ON" indicator
  explicitly — recommend adding one (e.g. a colored dot next to "ECU
  CONTROL" header) since relying on button color alone is not enough
  feedback for a safety-relevant state.
- Quick Tests tiles now active/tappable.

### 4.6 Injector Test
Top bar: `←` back, title "INJECTOR TEST".

**Select Injector**
- Row of 4 segmented buttons: "INJ 1" (selected/red-filled), "INJ 2",
  "INJ 3", "INJ 4" (unselected/outline).

**Parameters**
- "PULSE WIDTH (ms)" label + dropdown/stepper control showing "3.0".
- "TEST DURATION (s)" label + dropdown/stepper control showing "5".
- (Per SRS FR-MOD-3, these should be pre-filled from the active module
  profile's `[TEST_INJECTOR]` defaults when one is selected, editable
  regardless.)

**Action**
- Full-width green button: **START TEST** (becomes **STOP TEST**, red,
  while running — see §4.9).

**Injector Status card**
- "INJECTOR STATUS" header.
- Row: "Status" → value, e.g. "IDLE" (gray) / "TESTING..." (amber/yellow)
  when running.
- Row: "Last Result" → value (e.g. "—" until a test has run, then
  "PASS"/"FAIL").
- Row: "Response" → raw value/text from the driver (e.g. "OK" or a fault
  code/text).

### 4.7 Ignition Coil Test
Structurally identical to Injector Test (§4.6) with these differences:
- Selector row: "COIL 1"–"COIL 4".
- Parameter: "DWELL TIME (ms)" (in place of pulse width), value e.g. "3.0".
- Parameter: "TEST DURATION (s)", e.g. "5".
- Status card labeled "COIL STATUS" with the same Status/Last Result/
  Response rows.

### 4.8 All Injectors Test
Top bar: `←` back, title "ALL INJECTORS TEST".
- Large centered purple people/group icon.
- Descriptive text: "This will test all injectors sequentially."
- Parameters:
  - "PULSE WIDTH (ms)" — e.g. "3.0"
  - "TEST DURATION PER INJECTOR (s)" — e.g. "3"
- Full-width purple button: **START ALL TEST** (distinct color from the
  single-channel green Start, reinforcing this is the batch action).
- **Progress card**
  - "PROGRESS" header.
  - Row: "Status" → "IDLE" / "TESTING" / "COMPLETE".
  - Row: "Progress" → "0/4" incrementing as each injector completes.
- **Behavior:** sequentially runs the injector test routine per channel
  using the shared pulse width and per-injector duration; each channel's
  individual result should still be logged to the Results list as a
  distinct entry (per SRS FR-ALL-2, matches screenshot 10 showing "INJ 1
  Test" … "INJ 4 Test" as separate rows even though they came from an
  "all" run).

### 4.9 Test in progress (state of Injector Test screen)
Same layout as §4.6, in its running state:
- Selected injector segment stays highlighted (e.g. "INJ 1").
- **START TEST** button replaced by full-width red **STOP TEST** button.
- Injector Status card live values:
  - "Status" → "TESTING..." (amber).
  - "Time Elapsed" → running mm:ss counter (e.g. "00:00:03"). Note: this
    row's label differs from the idle state's "Last Result" row — the
    status card swaps its second row from "Last Result" to "Time Elapsed"
    while a test is actively running, then likely swaps back to show
    "Last Result" once the test completes (design confirms both rows exist
    in the finished card; sequence between them should be validated with
    the design owner during implementation).
  - "Response" → live value, e.g. "OK" (green) updating as telemetry
    arrives.
- **Behavior:** Stop Test sends an abort command; firmware should stop at a
  safe boundary (end of current pulse/dwell cycle, not mid-pulse — SRS
  FR-ALL-3 analog applies to single-channel stop too).

### 4.10 Test Results
Top bar: `←` back, title "TEST RESULTS".
- Scrollable list, each row:
  - Left: test name (e.g. "INJ 1 Test", "COIL 3 Test").
  - Right: result badge — "Pass" (green) or "Fail" (red) — plus a
    timestamp below it (e.g. "10:30:15"), right-aligned, smaller/gray.
- List shown in the reference screenshot: INJ 1–4 Test, COIL 1–4 Test, with
  INJ 3 and COIL 3 shown as "Fail" (red), all others "Pass" (green) —
  illustrates mixed results rendering correctly and that ordering is
  chronological (by the timestamps, ascending top-to-bottom).
- Two full-width buttons at bottom, stacked:
  - **CLEAR RESULTS** (red/outline or red-filled — screenshot shows solid
    red) — clears the current session's list after confirmation (recommend
    adding a confirm dialog since this is destructive to unsaved records,
    even though not shown in the reference shot).
  - **SAVE PDF REPORT** (dark red/maroon, with a document icon) — generates
    and saves/exports the PDF per SRS FR-RES-3.
- **Empty state (not shown, must design):** "No test results yet — run a
  test to see results here."

### 4.11 System Info (referenced by Home tile, screen not supplied — spec to fill the gap)
Not present in the reference screenshots; the Home dashboard's "System
Info" tile implies its existence. Recommended minimal content, consistent
with the rest of the app's card style:
- App version, firmware version (ESP32 + Nano), connected device identity.
- SD card status (present/free space) and last sync time.
- Supply voltage / DUT voltage / current, live (mirrors Nano `STATUS`
  telemetry) — useful for a technician sanity-checking the bench without
  opening a specific test screen.
- This screen should be treated as **draft/placeholder** until an actual
  reference design is provided — flag to the user before building it.

## 5. Interaction & state notes that apply across screens

- **Every destructive or power-affecting action** (ECU OFF while a test is
  running, Stop Test, Clear Results) should give immediate visual feedback
  within the existing ~250 ms status cadence (SRS NFR-PERF-1) — the UI
  should never appear to "hang" waiting on BLE round-trip.
- **Disconnection mid-test:** none of the reference screens show this state.
  Recommended behavior: show a non-blocking banner ("Connection lost —
  attempting to reconnect...") over the current screen rather than forcing
  navigation back to Home, since the Nano's independent heartbeat timeout
  is already handling the safety side (SRS FR-CONN-7).
- **Color semantics are consistent app-wide:** green = safe/on/pass, red =
  off/danger/fail/stop, amber = in-progress, purple = reserved for the
  batch "all injectors" action only (not reused elsewhere per the
  screenshots) — keep this reservation intentional in future screens rather
  than introducing purple for unrelated features.

## 6. Gaps to close before implementation

1. **System Info screen** — no reference design (§4.11).
2. **Settings screen** — referenced by bottom nav, no reference design
   (should at minimum cover: change PIN, forget device, units, app data
   reset per SRS FR-SYS-2).
3. **Tests tab (bottom nav)** — unclear if this duplicates the Home quick-
   tests grid or lists something additional (e.g. resistance/position tests
   from the existing module schema's `[TEST_RESISTANCE]`/`[TEST_POSITION]`
   sections, which have no screen yet at all).
4. **Empty/error states** — no-devices-found, incorrect PIN, BLE permission
   denied, disconnected-mid-test, empty results — none shown in references,
   all need explicit design.
5. **Module/vehicle selection screen** — screenshots never show picking a
   vehicle/module profile, yet test parameter defaults are expected to come
   from one (SRS FR-MOD-2/3). Needs a new screen (e.g. reachable from Home
   or a header dropdown) before that requirement can be implemented as
   specified.

# TIFF TESTER PRO app - user guide

The Android companion app for the TIFF TESTER PRO bench tester (app version
0.2.0). It connects to the tester, shows live bench readings, switches the relays,
runs tests, reads fault codes from a module over CAN / K-Line, and exports PDF
reports.

**Requirements:** an Android 8.0+ phone with Bluetooth. To install the app and
flash the tester see [FLASHING_GUIDE.md](FLASHING_GUIDE.md); to build and check the
hardware see [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md) and
[BRINGUP_CHECKLIST.md](BRINGUP_CHECKLIST.md).

## What works today (read this first)

The app and firmware are a bench prototype and **have not been validated on real
hardware yet**.

| Feature | Status |
|---|---|
| Live data (voltages, current, position, temperature, readiness) | Works |
| DUT relay on/off, ALL OUTPUTS OFF, auxiliary relays, hold-to-energise, relay tests | Works |
| Protection settings (over-temperature trip, aux auto-off) | Works |
| Sensor quick tests (resistance, short-to-ground, current monitor) | Work, if the selected module enables them |
| Live graph, CSV copy, PDF report | Works |
| CAN / UDS / K-Line / KWP diagnostics (read-only) | Implemented, **unvalidated against a real ECU** |
| **Injector, ignition-coil and all-injectors tests** | **Do not drive anything.** There is no injector/coil driver hardware yet, so these screens always report `NOT_IMPLEMENTED`. |
| MOSFET outputs | Shown disabled - no driver hardware yet |

There are two firmware lines the app can talk to. **The main system (BLE) has every
feature above. The V2 line (Bluetooth Classic) has fewer** - see
[section 12](#12-v2-board-differences).

The app is not a safety device. The Arduino Nano on the tester does the real
protection; the app only sends requests.

## 1. Safety

- **Start with a dummy load**, not a real module, and a current-limited supply.
- **ALL OUTPUTS OFF** (red button, Home and Controls) immediately drops the DUT relay
  and every auxiliary relay and stops any running test. It has no confirmation on
  purpose. Use it first, ask questions after.
- The hardware **e-stop** works regardless of the app.
- **If the phone disconnects, the DUT stays powered.** Only losing the tester's ESP32
  or a protection trip cuts power. Don't walk away from a powered DUT.
- A fault (under/over-voltage, over-current, over-temperature, e-stop, link loss)
  turns everything off and latches. Nothing comes back on by itself; clear the cause,
  press **RESET FAULT** (Controls), then switch things on again.
- Diagnostic requests are **blocked while the e-stop or a fault is active**.

## 2. Connecting

### 2.1 First launch

The app asks for Bluetooth ("nearby devices") permission before scanning. Without
it, scanning does nothing and a message says so.

### 2.2 Scan screen (Home > SCAN FOR DEVICES)

- Lists nearby BLE devices, strongest signal first; tester boards (name starting
  `TiffTester`) are sorted to the top with a green signal icon.
- **Show unnamed devices** reveals the rest.
- A **PAIRED (BLUETOOTH CLASSIC)** section lists V2 boards you have already paired in
  Android's Bluetooth settings (any paired device whose name contains "TIFF", for
  example `TIFF_TESTER_V2`). V2 boards are not found by scanning - pair them in
  Android settings first, then they appear here.
- Tap a device to connect. **SCAN FOR DEVICES / STOP SCAN** restarts or stops the scan.

### 2.3 PIN screen (BLE boards)

- Enter the tester's PIN and press **CONNECT**. The factory PIN `TIFF2026` is shown
  in red on this screen as a reminder; **change it** (section 11).
- **The app cannot tell whether the PIN was right.** A wrong PIN still looks
  "connected", but the tester ignores every command. If nothing responds after
  connecting, disconnect and reconnect with the correct PIN.
- Five wrong PINs in a row lock the tester out for 30 seconds, growing with each
  further failure.
- V2 boards have no PIN.

To disconnect, use **DISCONNECT** in the Home Connection card.

## 3. Home screen

From top to bottom:

1. **Red banner** - appears when you connected with the factory PIN. Tap it to open Settings.
2. **Connection** - status, device name, and the connect/disconnect button.
3. **Live Data** - readings and readiness (section 4).
4. **Module** - the active module profile and **SELECT MODULE** (section 6).
5. **Vehicle & Module Database** - your own records (section 10).
6. **DUT Relay** - **DUT ON / DUT OFF**, **FULL CONTROLS**, **ALL OUTPUTS OFF**.
7. **Diagnostics** - **OPEN DIAGNOSTICS** (section 9).
8. **Quick Tests** - Injectors, Coils, All Injectors (not implemented, section 7) and System Info.

The bottom bar has four tabs: **Home, Tests, Results, Settings**.

**DUT ON** is a request: the tester checks supply voltage (11-15 V), current and the
e-stop first and refuses if anything is wrong. Look at the Live Data **SYSTEM** line
to see why.

## 4. Live data

The Live Data card shows, in this layout:

```
Supply voltage - 12.0 V
DUT voltage    - 11.8 V
DUT current    - 3.00 A
Position       - 42 %
Temperature    - 31.5 °C

CAN      READY ✓
K-LINE   READY ✓
INA219   READY
SYSTEM   READY
```

- Values show `--` until you are connected (never a misleading zero).
- **Position** is a raw 0-100 % reading of a 0-3.3 V input. With nothing wired it
  reads 0 %, which is not a real position.
- **Temperature** needs a sensor fitted on the tester; without one the reading is
  meaningless. On V2 boards it shows `n/a` (V2 does not report degrees C).
- DUT voltage and current come from the INA219 when one is fitted, otherwise from the
  tester's own sensors.
- **CAN / K-LINE:** `READY` means the interface initialised. `READY ✓` means its
  self-test also passed; `SELF-TEST FAILED` is shown in red if it did not. K-LINE
  `READY` does not mean an ECU answered. `NOT READY` means the interface did not start.
- **INA219:** `READY` or `NOT FOUND`.
- **SYSTEM:** `READY` (all good), `NOT READY`, `OFFLINE` (not connected), or
  `FAULT: <reason>`. Reasons you may see: `ESTOP`, `UNDERVOLTAGE`, `OVERVOLTAGE`,
  `DUT_OVERVOLTAGE`, `OVERCURRENT`, `OVERTEMPERATURE`, `ESP32_HEARTBEAT_TIMEOUT`.

### 4.1 Live graph

**VIEW GRAPH** opens charts of supply and DUT voltage, DUT current, DUT power,
position and temperature, newest on the right. The top bar can **copy the data as
CSV** (same columns as the tester's own log) or clear it. It keeps the last 600
readings (about five minutes) in memory for the session; the data survives a
disconnect so you can still make a report afterwards, but it is lost when the app is
closed.

## 5. Controls screen (Home > FULL CONTROLS)

- **Live Data** at the top.
- **DUT Relay (Relay 1)** - state, **DUT ON / DUT OFF**, **RESET FAULT**, **TEST RELAY 1**.
  The relay test cycles the relay three times and checks the DUT voltage follows it;
  the DUT rail must be connected for a PASS. The result appears on the Results tab.
- **Auxiliary Relays (2-4)** - these drive other bench outputs, not the DUT. Each row
  has:
  - a **switch** to turn it on or off;
  - **HOLD** - the relay is on only while you keep your finger on the button, and
    drops when you let go (the tester also releases it by itself if the phone stops
    sending, so a lost release cannot leave it on);
  - **TEST** - cycles it three times. Relays 2-4 have no feedback sensor unless you
    wired one, so the result is `ACTUATED` ("the cycles ran"); you confirm the clicks
    or the load yourself.
  - Rows use your own relay names if the active module record has them (section 10).
  - All auxiliary relays also turn off on an e-stop, any fault, loss of the tester's
    controller, and **ALL OUTPUTS OFF**.
- **Relay Polarity** - shows whether the relay module is `ACTIVE-LOW` (default) or
  `ACTIVE-HIGH` and lets you switch it, after a confirmation. **A wrong setting
  energises relays that should be off.** All relays are forced off before and after
  the change. It must match your relay module.
- **Protection**
  - *Current limit* - read-only. 5.0 A is the hard cap; the selected module's own limit
    appears here when it is lower (a module can only tighten it).
  - *Over-temperature trip* - Off, or 50 / 60 / 70 / 85 / 100 °C. **Enable it only
    after a temperature sensor is fitted**, otherwise the reading is meaningless.
  - *Aux relay auto-off* - Off, or 10 / 30 / 60 / 300 s, so a forgotten auxiliary
    relay switches itself off. Applies to relays switched on after you set it.
  - Both settings are saved on the tester.
- **MOSFET Outputs** - shown disabled; the driver hardware does not exist yet.
- **ALL OUTPUTS OFF**.

If a switch does nothing, check the Live Data **SYSTEM** line - an e-stop or fault
blocks switching on.

## 6. Selecting a module

Home > **SELECT MODULE** lists the module profiles on the tester's SD card (refresh
with the arrow). Tap one; a green check shows the active profile. Selecting a module:

- loads its limits (voltage window, maximum current) into the tester's protection;
- sets the CAN / K-Line addressing used by Diagnostics;
- enables the sensor tests that module allows.

Profiles live on the **SD card** in the tester (`/MODULES/<id>.INI`); the app cannot
create or edit them. They are separate from your phone-side records (section 10).

## 7. Tests tab

**Channel Tests** - Injector Test, Ignition Coil Test, All Injectors Test. Each has
channel buttons and sliders (pulse width or dwell time, duration) and START / STOP.
**These do not fire anything today** - there is no driver hardware - and report
`NOT_IMPLEMENTED`. They are kept so the workflow is in place when the hardware
exists.

**Sensor Quick Tests** use the tester's voltage and current sensors, so the results
are real:

| Test | Passes when |
|---|---|
| Resistance | measured ohms (DUT voltage / current) is between the module's min and max |
| Short-to-ground | current stays below 90 % of the module's maximum |
| Current monitor | current is at or below the module's maximum |

They need a **module selected whose profile enables that test**, and the DUT
powered (resistance needs current flowing). Otherwise the result is a fail with
`NOT_ENABLED_FOR_MODULE` or `NO_CURRENT_FLOW`. The result appears on the Results tab.

## 8. Results tab

Newest first, with Pass/Fail and the time. Relay tests appear here as "Relay 2 Test"
and so on.

- **CLEAR RESULTS** (asks first) clears the list.
- **SAVE PDF REPORT** opens the phone's share sheet. The PDF has a header, one row per
  result, a summary, and - if you have been connected a while - a **Live data** table
  with the minimum, average and maximum of each reading.
- Up to 200 results are kept on the phone between sessions.
- **Times are the phone's clock** at the moment the result arrived; the tester has no
  clock.

## 9. Diagnostics (Home > OPEN DIAGNOSTICS)

BLE tester only. Everything is **read-oriented**: the tester refuses programming,
security access, routine control and ECU resets no matter what you type. Requests are
blocked during an e-stop or fault. Unvalidated against real ECUs - check with an
analyser first.

The bottom half is the **console**: green lines are results, red lines are errors,
grey `> ...` lines are what the app sent; the bin icon clears it. **STOP** (top right)
stops monitors and drops any queued request. Leaving the screen stops monitors.

### CAN tab
- **Self-test** - **RUN SELF-TEST** checks the CAN controller internally (nothing is
  sent on the bus). **+ K-LINE ECHO** also checks the K-Line transceiver by briefly
  pulling the line low - not while an ECU is mid-conversation. The same checks run at boot.
- **CAN bus** - pick the speed and the **MCP2515 crystal (8 or 16 MHz)**, then
  **CAN INIT**. The crystal must match the one printed on your CAN board or timing
  is wrong. Saved on the tester.
- **Diagnostic addressing** - TX ID, RX ID (hex), 29-bit switch, **ISO-TP padding**
  (pad byte AA/00/55/CC, or none), **APPLY IDS**. If requests time out with correct IDs,
  try another padding option.
- **Monitor and raw transmit** - the **CAN monitor** switch streams received frames;
  **SEND RAW FRAME** transmits one frame (asks first). Raw frames go straight onto the
  bus: bench modules only, never a vehicle network.

### UDS tab (CAN)
**START SESSION** (default or extended), **READ DID** (for example `F190`),
**READ DTC** (codes shown as P/C/B/U numbers), **TESTER PRESENT**, **CLEAR DTC**
(asks first), and a raw request box. Only services 10, 3E, 22, 19 and 14 are accepted.

### K-LINE tab
Baud (9600 / 10400), ECU and tester addresses, **APPLY**, **FAST INIT**,
**5-BAUD INIT** (takes about 2.5 s), a raw K-Line monitor, and KWP2000 buttons:
**START SESSION, TESTER PRESENT, READ DTC, CLEAR DTC** (asks first) and a raw box.
Allowed KWP services: 10 81, 3E, 18, 17, 21, 1A, 14, 82.

If a module has no addressing set, a red warning appears before the first request
saying generic addresses are in use and are unverified. The app pre-fills the
addressing from the active module's record (section 10) when it has one.

## 10. Vehicles and Modules (your own records)

Home > **VEHICLES** and **MODULES**. These are notes you keep **on the phone only**;
they are not sent to the tester.

- Tap **+** to add, tap a record to edit; the menu on each row has **Duplicate** and
  **Delete** (asks first). Both lists have a search box.
- **Vehicle:** manufacturer and model (required), year, engine, fuel type, engine code,
  ECU info, protocol, notes.
- **Module:** name, vehicle, type, communication protocol; CAN speed, TX/RX IDs,
  29-bit option, ISO-TP padding, K-Line baud and addresses; voltage, current, position
  and temperature limits; relay requirements and **names for relays 1-4**; MOSFET
  requirements; test procedure, diagnostic commands, pass/fail criteria, notes.
- The **SD module id** field links a record to a profile on the tester's SD card.
  When that profile is active, the record's relay names appear on the Controls screen
  and its CAN / K-Line settings pre-fill Diagnostics.

## 11. Settings tab

- **BLE Pairing PIN** - when connected, type a new PIN (at least 6 characters) and
  **UPDATE PIN**. Change the factory PIN `TIFF2026` before the bench leaves your desk.
- **Forget last device** - forgets the remembered device and disconnects.
- **Clear app data** - clears the **test results** and the remembered device. It does
  **not** delete your vehicle or module records.

The Wi-Fi password is not set here; see [FLASHING_GUIDE.md](FLASHING_GUIDE.md) section 9.
**System Info** (Home > Quick Tests) shows the app version, the connected device and
the live bench status.

## 12. V2 board differences

Connect a V2 board from the Scan screen's Paired section (no PIN).

| Area | On V2 |
|---|---|
| Live data | Supply/DUT voltage, current, position, INA219/CAN/K-LINE status. **Temperature shows n/a.** |
| DUT ON / OFF, ALL OUTPUTS OFF | Work |
| Relays | The four Nano relays are **relays 1-4** on the Controls screen (the DUT relay is separate). Their state is read back from the Nano on V2.2.1 firmware; older V2.2 shows the last command sent, and the screen says so. |
| Hold-to-energise, relay tests, relay polarity, protection settings, self-test | **Not available** (disabled) |
| Diagnostics | **Not available** - a banner says so; V2 only checks the CAN chip and opens the K-Line port |
| Module select | Works, using V2's own module-file format |

## 13. Troubleshooting

| Problem | Likely cause | What to do |
|---|---|---|
| Scan finds nothing | Permission denied, Bluetooth off, tester not powered/advertising | Grant Bluetooth permission; check the tester's boot log shows `BLE: advertising` |
| V2 board not in the list | Not paired | Pair `TIFF_TESTER_V2` in Android Bluetooth settings first |
| "Connected" but nothing responds | **Wrong PIN** (the app cannot tell) | Disconnect, reconnect with the right PIN. After five wrong tries, wait 30 s |
| `Failed to reach device` | Link dropped | Reconnect |
| DUT ON / a relay won't switch on | Fault or e-stop active, supply out of 11-15 V, or the tester refused | Read Live Data **SYSTEM**; fix the cause; **RESET FAULT** |
| Relay came on when it shouldn't | Relay polarity wrong for your module | Controls > Relay Polarity (and see the bring-up checklist 3.1) |
| Everything switched off by itself | A protection trip (read the fault text) or the e-stop | Fix the cause, RESET FAULT |
| Quick test shows `NOT_ENABLED_FOR_MODULE` | No module selected, or its profile doesn't enable that test | Select a module whose profile enables it |
| Injector/coil test says `NOT_IMPLEMENTED` | No driver hardware exists yet | Expected |
| Diagnostics requests time out | Wrong CAN IDs, speed, crystal, or padding; no ECU; e-stop/fault active | Check each (section 9); run the self-test |
| `SELF-TEST FAILED` | Controller/transceiver unpowered or wired wrong | Check the CAN/K-Line wiring and power; see the checklist |
| Graph is empty | Not connected long enough | Wait for readings; graph fills as status arrives |
| PDF has no live-data table | No readings captured this session | Connect and let it run first |
| Position reads 0 % | Nothing wired to the input | Normal when unconnected |

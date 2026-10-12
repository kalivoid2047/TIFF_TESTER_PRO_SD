# Troubleshooting the tester

For when the bench misbehaves. It is organised around **what the tester reports**
(fault text, boot-log lines, error codes), because those are exact strings you can
look up. The causes listed are the likely ones; **none of this firmware has been
validated on real hardware yet**, so treat an unexpected symptom as information, not
as a user error.

Related: [BRINGUP_CHECKLIST.md](BRINGUP_CHECKLIST.md) (first power-up),
[WIRING_DIAGRAM.md](WIRING_DIAGRAM.md), [FLASHING_GUIDE.md](FLASHING_GUIDE.md),
[APP_USER_GUIDE.md](APP_USER_GUIDE.md).

**Safety first:** if anything smokes, smells hot, or a relay acts on its own - e-stop,
power off, unplug. Troubleshoot with a **dummy load**, not a real module.

---

## 1. First five minutes (triage)

1. **Make it safe.** Press **ALL OUTPUTS OFF** in the app, or hit the e-stop.
2. **Read the SYSTEM line** on the app's Live Data card (or `http://192.168.4.1/api/live`
   over Wi-Fi). It says `READY`, `NOT READY`, `OFFLINE`, `NANO OFFLINE` or
   `FAULT: <reason>`.
3. **If there is a fault reason**, go to section 3.
4. **If everything reads zero / `--` / `NANO OFFLINE`**, the Nano is not talking to the
   ESP32 - go to section 6.1.
5. **If it only happens with the DUT connected**, reproduce it on the dummy load. If it
   goes away, the problem is the DUT or its wiring, not the tester.
6. **Change one thing at a time**, and write down what you changed.

## 2. How to see what the tester is telling you

| Source | What it shows |
|---|---|
| **Buzzer** (Nano D6) | Sounds while a fault is latched; stops on `RESET FAULT` or DUT OFF |
| **Nano LED** (D13) | On while the **DUT relay (relay 1)** is energised (not the aux relays) |
| **ESP32 LED** (GPIO2) | Blinks about once a second while its main loop runs. Solid on/off = hung or not running |
| **Relay module LEDs** | Which relays are energised (the quickest polarity check) |
| **App** | Live Data (SYSTEM, CAN/K-LINE/INA219), Diagnostics console, Results |
| **ESP32 USB serial, 115200** | Boot log, self-test results, and every diagnostics line |
| **Nano USB serial, 115200** | `STATUS,...`, `EXT,...`, `LIMITS,...` every 500 ms; accepts commands (below). **It is the same UART as the ESP32 link**, so read it with the ESP32 detached. The Nano prints nothing at boot. |
| **Web UI** `http://192.168.4.1/` | Live data, relay buttons; `/api/status` (one line) and `/api/live` (text) |
| **SD card `/LOGS/`** | `system.log`, `tests.log`, `DIAG.CSV`, `TEST_<ms>.CSV` (one per DUT-on session) |

**Handy Nano serial commands** (newline-terminated): `STATUS`, `GET_CONFIG`, `HEARTBEAT`,
`POWER_ON`, `POWER_OFF`, `RESET_FAULT`, `RELAY,<n>,<0|1>`, `RELAY_TEST,<n>`.
`STATUS,<relay>,<fault>,<estop>,<watchdog>,<supply>,<dut>,<current>,<faulttext>`.
Aux relays and the DUT relay drop after ~2 s without `HEARTBEAT`, so send one every
second while you test by hand.

---

## 3. Fault messages (the Nano cut the outputs)

A fault turns every relay off and latches until cleared. Fix the cause, press
**RESET FAULT**, then switch on again. `ESTOP` also needs the e-stop released.

| Fault text | Meaning | Likely causes | What to check |
|---|---|---|---|
| `ESTOP` | D3 sees the e-stop as active | E-stop pressed; **wire broken or unplugged** (fail-safe wiring treats open as stopped); D3 not jumpered when no e-stop fitted; NO button wired instead of NC | Jumper D3 to GND: if the fault clears, the e-stop circuit is open |
| `UNDERVOLTAGE` | Supply below the minimum (11 V, or the module's higher minimum) | Bench supply sagging or in current limit; divider not calibrated; voltage drop in the feed; module profile minimum above your supply | Meter the supply at the divider input; calibrate `CAL_SUPPLY`; raise the supply current limit |
| `OVERVOLTAGE` | Supply above the maximum (15 V, or the module's lower maximum) | Supply set too high; divider miscalibrated | Meter it; `CAL_SUPPLY` |
| `DUT_OVERVOLTAGE` | DUT node above 15 V | DUT sense divider miscalibrated; something feeding voltage back | Meter the DUT node; `CAL_DUT` |
| `OVERCURRENT` | Current above the limit (5 A, or the module's lower limit) | Real overload or short; **uncalibrated current zero/scale**; module inrush at turn-on (the check runs right after the relay closes, with no grace period); module limit set too low | Test on the ~12 Ω load; do `CAL_CURRENT_ZERO` (no load) and `CAL_CURRENT_SCALE`; read the module's `max_current_a` |
| `OVERTEMPERATURE` | Temperature above the trip you set | Real heat; **no sensor fitted**, wrong sensor type (`TEMP_C_PER_V`), or a floating A3 input | Set the trip to Off unless a real sensor is wired |
| `ESP32_HEARTBEAT_TIMEOUT` | No heartbeat from the ESP32 for 2 s | ESP32 reset, crashed or browned out; UART wire/level problem; **you were testing by hand and stopped sending `HEARTBEAT`** | Section 6.1 and 8 |
| `POLARITY_CHANGE` | You changed relay polarity or ran `RESET_CALIBRATION` | Expected | Just `RESET FAULT` |

Other things that look like faults:
- **DUT ON does nothing and there is no fault text:** the tester refused (supply out
  of 11-15 V, e-stop) - the refusal produces the same texts above; check SYSTEM, and
  that you are actually connected with the right PIN (section 6.2).
- **`PARAM_EXCEEDS_MAX`:** a test window was asked for a pulse wider than 8 ms.
- **Everything switched off at once after a while:** read the fault text; if there is
  none and the link was lost, it was the heartbeat timeout.

---

## 4. Relays

| Symptom | Likely cause | Fix |
|---|---|---|
| **A relay is on at power-up** | Relay-module polarity doesn't match the firmware (module is active-HIGH, firmware default is active-LOW) | `SET_RELAY_POLARITY,0` (or Controls > Relay Polarity). Do not continue until all relays are off at boot |
| No relay responds, module LEDs dark | Module VCC/GND missing, or its isolation jumper (JD-VCC) is wrong | Check the 5 V supply and grounds; the coil supply must not come from a Nano pin |
| LED lights, no click, no load | Coil under-powered (USB-powered Nano, weak 5 V) | Power coils from the 5 V buck rail |
| Clicks, but the load gets no power | Load wired to NC instead of NO, or the 12 V path is broken (fuse) | COM/NO for "on = powered"; meter across the contacts |
| Switch turns on, then off within ~2 s | Heartbeat lost (when testing from a serial terminal) | Send `HEARTBEAT` every second, or use the app |
| Aux relay switches off by itself after a set time | Aux relay auto-off is enabled | Controls > Protection > Aux relay auto-off = Off |
| Won't switch on from the app | E-stop or latched fault, supply over 15 V (aux), or any limit exceeded (DUT relay) | SYSTEM line; `RESET FAULT` |
| **HOLD** does nothing | Needs the BLE tester, a correct PIN and an active connection; disabled on V2 | Reconnect; check the PIN |
| `RELAY_TEST ... ACTUATED` | Relays 2-4 have no feedback; the cycles ran | **Not a pass** - confirm the clicks/load yourself, or wire feedback inputs |
| Relay 1 test `FAIL`, detail `NO_DUT_VOLTAGE_WHEN_ON` | DUT node didn't rise to half the supply while on | DUT output connected? DUT divider/calibration; blown fuse |
| Relay 1 test `FAIL`, detail `DUT_VOLTAGE_WHEN_OFF` | DUT node still above 2 V after 0.6 s off | DUT capacitance holding the rail; welded contacts; back-feed. Meter it |
| Aux test `FAIL` with `NO_FEEDBACK_WHEN_ON` / `FEEDBACK_WHEN_OFF` | Feedback inputs enabled, sense wire missing/wrong polarity | Check D2/D11/D12 wiring and `AUX_FB_ACTIVE_LOW` |

**Relay test `ABORTED` details:** `BAD_RELAY` (n not 1-4), `TEST_WINDOW_ACTIVE`,
`ALREADY_RUNNING`, `RELAY_ALREADY_ON` (switch it off first), `NOT_SAFE` / `ESTOP` /
a fault name / `REFUSED` (a safety check failed), `STOPPED`, `POWER_OFF` (you stopped it).

---

## 5. Wrong or odd readings

| Symptom | Likely cause | Fix |
|---|---|---|
| Supply/DUT voltage off by a constant ratio | Divider ratio isn't the 4.0 the firmware assumes | Meter the divider output (12 V should give ~3 V at 4:1); `CAL_SUPPLY,<volts>`; `CAL_DUT,<volts>` with the relay on |
| DUT voltage reads 0 with the relay on | DUT node not connected to A0; `CAL_DUT` needs a raw reading above 0.05 V | Check wiring; meter A0 |
| Current is non-zero with no load | Zero offset not calibrated, or ACS712 not on 5 V | DUT off, no load: `CAL_CURRENT_ZERO` (sensor should sit near 2.5 V) |
| Current scale is wrong | Different ACS712 variant (5 A: 185 mV/A, 20 A: 100, 30 A: 66) | `CAL_CURRENT_SCALE,<amps>` with a known load |
| Current always positive | By design - the Nano reports the absolute value | - |
| Readings noisy | Poor common ground, long unshielded sense wires | Star the grounds; shorten/twist; add 100 nF at the ADC pins |
| Temperature absurd | Wrong sensor type, or A3 floating | Fit the sensor / set `TEMP_C_PER_V`; trip Off meanwhile |
| Position stuck at 0 % | Nothing wired - the input has a pull-down | Normal when unconnected |
| `INA219 NOT FOUND` | Wiring, power, address (0x40), missing pull-ups | Check 3.3 V, SDA/SCL (GPIO21/22); the ESP32 retries every 5 s |
| INA219 disagrees with the meter | Wrong shunt value; current above ~3.2 A saturates the range; the display shows the INA219 when present | Compare at a 1 A load |
| **Display looks fine but the Nano still trips** | The app shows INA219 values; the Nano's protection uses its **own** sensors | Calibrate the Nano (`CAL_*`); the two can disagree |
| Values all zero but the app says connected | The Nano isn't answering the ESP32 | Section 6.1 (the app shows zeros, not `--`, when the link to the Nano is down) |

---

## 6. Links and connectivity

### 6.1 Nano <-> ESP32 link

Symptoms: all readings zero, SYSTEM `NOT READY` (app) or `NANO OFFLINE` (`/api/live`),
relays don't respond, and eventually `ESP32_HEARTBEAT_TIMEOUT` on the Nano.

| Check | How |
|---|---|
| Nano alone is alive | Detach the ESP32; the Nano's USB serial (115200) should print `STATUS,...` lines |
| Wiring | Nano **D1 (TX)** -> divider -> ESP32 **GPIO16 (RX2)**; ESP32 **GPIO17 (TX2)** -> Nano **D0 (RX)**; common ground. TX/RX swapped is the usual mistake |
| Level divider | Nano TX is 5 V; the 1k/2k divider must be between D1 and GPIO16 |
| Baud | Both ends 115200 (the firmware does this - only a hand-edited sketch breaks it) |
| Flashing leftovers | A Nano with the USB-serial adapter or a monitor attached holds D0/D1 |
| Activity | A scope/logic probe on GPIO16 should show a burst every 500 ms |

### 6.2 Bluetooth (BLE)

| Symptom | Likely cause | Fix |
|---|---|---|
| Tester not in the scan list | Not advertising (ESP32 not booted, serial shows no `BLE: advertising`), another phone already connected (one client at a time), permissions | Check the boot log; disconnect the other phone; grant permissions (Android 11 and below also need location on) |
| Connects, then drops | Range/interference; power sag resetting the ESP32 | Move closer; check for resets (section 8) |
| **"Connected" but nothing happens** | **Wrong PIN** - the app can't tell | Reconnect with the right PIN. Five wrong tries = 30 s lockout, growing |
| Live data stutters/stops | Status notifications are ~300 bytes and need Android's larger MTU; a failed negotiation drops them | Disconnect/reconnect; toggle phone Bluetooth |
| Controls disabled / `Failed to reach device` | Link dropped | Reconnect |

### 6.3 Wi-Fi / web UI

| Symptom | Fix |
|---|---|
| No `TIFF_TESTER` network | Check the boot log for `BOOT: Wi-Fi AP started.`. `Wi-Fi AP FAILED` usually means a **password under 8 characters** in `/CONFIG.INI` (the API enforces 8; a hand-edited file may not) |
| Wrong password | Default `tifftester`. Forgotten: delete `/CONFIG.INI` on the SD card and power-cycle (back to defaults) |
| Phone says "no internet" and hops off | Keep it connected; the tester has no internet |
| Page won't load | Use `http://192.168.4.1/` (not https) |
| Changed the Wi-Fi password but the old one still works | The new one applies after a **reboot** |

### 6.4 SD card

| Symptom | Fix |
|---|---|
| `SD init failed.` at boot | Reformat FAT32 (<= 32 GB is safest), insert **before** power-up, check GPIO5 and the shared SPI lines. If it only fails with the MCP2515 connected, suspect a floating/shorted CAN CS on the shared bus |
| No logs/reports | SD not ready (above) |
| Module not listed | The file must be `/MODULES/<id>.INI` at the **top level** (no subfolders, `.INI` extension). The repo example sits in a subfolder - copy it up |
| Web upload `REJECTED` | The file needs `[MODULE]`, `id=`, `max_current_a=`, `min_voltage_v=`, `max_voltage_v=` |
| Quick test `NOT_ENABLED_FOR_MODULE` | The profile doesn't enable that test, or no module is selected |

---

## 7. Diagnostics (CAN, UDS, K-Line)

Diagnostics are read-only and **unvalidated against a real ECU**. Work bottom-up: bus
timing first, then addressing, then services.

### 7.1 CAN bus

| Message | Meaning | Check |
|---|---|---|
| `CAN: MCP2515 not responding` (boot) | SPI can't reach the controller | Wiring (GPIO15 CS, 18/19/23), module power/logic level |
| `can_init FAILED ...` | Bad bitrate/clock, or controller not answering | Use 125k/250k/500k/1M and **8 or 16 MHz** only |
| Self-test: `no frame received in loopback` / `cannot enter ...` | Controller or SPI fault (the bus isn't involved) | SPI wiring, power, crystal |
| `TX FAILED` / `CAN_TX_FAILED` with `EFLG=...` | Frame never got sent (no ACK on the bus) | Another node on the bus? Termination? Same speed? |
| `BUS_OFF`, `TX_ERR_PASSIVE`, `RX_ERR_PASSIVE`, `ERR_WARN` in the status | Controller error counters high | **Wrong bitrate or crystal**, missing 120 Ω termination, CAN_H/CAN_L swapped, no other node |
| `RX_OVERFLOW` | Frames arriving faster than they are read | Busy bus; the monitor is polled, not interrupt-driven |
| Monitor shows nothing | Wrong speed/crystal, no traffic, wiring | Compare with a USB-CAN analyser on the same pair |

The **crystal** (8 vs 16 MHz, printed on the MCP2515 board) is the commonest cause of a
silent bus. The default is 8 MHz.

### 7.2 ISO-TP and UDS

| Message | Meaning | Check |
|---|---|---|
| `... FAILED: TIMEOUT` | No answer at all | **TX/RX IDs**, speed, crystal, padding, ECU awake/in the right state |
| `NO_FLOW_CONTROL` | Multi-frame request, ECU never answered the first frame | IDs, padding |
| `FC_OVERFLOW`, `FC_WAIT_TIMEOUT` | ECU can't accept the message / kept saying wait | ECU state |
| `CF_TIMEOUT`, `BAD_SEQUENCE`, `BAD_SINGLE_FRAME` | Broken or interleaved multi-frame response | Noisy bus; another node using the same IDs |
| `BAD_LENGTH` | Request was empty or over 4095 bytes | Shouldn't come from the app; check a hand-typed raw request |
| `RESPONSE_TOO_LONG` | Reply over 256 bytes | Ask for less |
| `CAN_NOT_READY` | Controller not initialised | `CAN INIT` first |
| `TOO_MANY_PENDING_RESPONSES` | ECU kept answering "pending" (NRC 0x78) | ECU busy |
| `BLOCKED_SERVICE`, `BLOCKED_SESSION` | **By design**: only services 10 (default/extended), 3E, 22, 19, 14 are allowed | - |
| `BAD_REQUEST_FORMAT`, `EMPTY_REQUEST`, `BAD_HEX` | Malformed request text | Hex bytes only |
| `SAFETY_BLOCK` | E-stop or fault active | Clear it first |
| `BUSY (command queue full)` | Too many requests queued | Press STOP, retry |
| `WARNING: no CAN IDs set ... unverified` | Generic 0x7E0/0x7E8 in use | Put the module's real IDs in its profile |

**A `NEGATIVE ... (name)` reply is good news:** the ECU answered, so timing, IDs and
padding are right. Typical codes: `0x11` service not supported, `0x12` sub-function not
supported, `0x13` bad length/format, `0x22` conditions not correct, `0x31` request out
of range, `0x33` security access denied, `0x7E/0x7F` not supported in this session
(start the extended session first).

If you get `TIMEOUT` with IDs you trust, try the other **ISO-TP padding** options
(pad byte AA/00/55/CC, or none) before anything else.

### 7.3 K-Line / KWP

| Message | Meaning | Check |
|---|---|---|
| Self-test `RX held low at idle` | Transceiver unpowered/missing or line shorted/held low | L9637D VBAT and VCC; disconnect the DUT and re-run |
| Self-test echo `RX did not follow TX low` | TX isn't reaching the line or RX isn't wired | GPIO25/26, L9637D power |
| `FAST_INIT FAILED: NO_RESPONSE` | ECU silent | Target/source address, baud (9600 vs 10400), ECU actually K-Line, ignition/power |
| `BAD_STARTCOMM_RESPONSE` | Something answered, but not StartCommunication | Wrong protocol or address |
| `5BAUD_INIT FAILED: NO_SYNC_BYTE` | No 0x55 after the 5-baud address | ECU address; baud; wiring |
| `NO_KEY_BYTES`, `NO_ADDRESS_ECHO` | Handshake broke part-way | Timing/noise; try again |
| `BAD_FRAME`, `NO_RESPONSE` on a request | Garbled or no reply | Session started? Baud? Scope the line |
| `BAD_BAUD` | Only 9600 and 10400 are supported | - |
| `BLOCKED_SERVICE` / `BLOCKED_SESSION` | By design (allowed: 10 81, 3E, 18, 17, 21, 1A, 14, 82) | - |

K-LINE `READY` only means the port is open; use the self-test and a scope to judge the
line, and expect init timing to need tuning per ECU.

---

## 8. Resets, brownouts and the heartbeat

- **ESP32 keeps resetting** (serial shows `Brownout detector was triggered`, or the
  boot log repeats): a supply problem is common on builds like this - relay-coil
  switching, a weak USB or shared 5 V rail, or Wi-Fi/BLE transmit peaks. Power the ESP32
  and the relay coils from the buck converter, not USB; add bulk capacitance at the ESP32's
  5 V input. (A general cause on this class of build - not yet confirmed on your bench.)
- **Every ESP32 reset drops the outputs** within ~2 s via the heartbeat timeout - that is
  the dead-man switch working. If it happens while you are *not* touching anything,
  look for the reset cause first.
- **Nano resetting:** its hardware watchdog restarts it if its loop stalls for 2 s; it
  boots with every relay off.
- **Long diagnostics exchanges and K-Line init** (~2.5 s) keep the heartbeat going, so
  they should not drop the DUT. If one does, report it (section 10).

---

## 9. V2 line

The V2 sketch replies in `KEY|VALUE` style; the app shows these in its messages.

| Reply | Meaning | Check |
|---|---|---|
| `ERROR|NANO_OFFLINE` | ESP32 can't reach the Nano over I2C | Address 0x12, **level shifter**, SDA/SCL, common ground, Nano flashed |
| `OK|RELAY2=ON|CONFIRMED` | Worked | - |
| `ERROR|RELAY2=ON_NOT_CONFIRMED|NANO_FAULT=YES` | Nano refused (e-stop, or stale heartbeat) | D3 e-stop circuit; heartbeat |
| `ERROR|..._NOT_CONFIRMED|NANO_FAULT=NO` | Nano didn't switch it | No heartbeat in the last 1.5 s? Wiring |
| `BLOCKED|FAULT_ACTIVE=<reason>` | A fault is latched | `RESET_FAULT` after fixing it |
| `PRETEST=FAIL|NO_MODULE_SELECTED` / `MODULE_NOT_VALIDATED` | Select and `VALIDATE_MODULE` first | V2 module format |
| `PRETEST=FAIL|NANO_OFFLINE` / `NANO_FAULT` / `INA219_NOT_READY` / `SD_NOT_READY` / `SUPPLY=<v>` | The named precondition failed | **INA219 is required on V2**; supply must be 11-15 V |
| `FAULT|DUT_OVERCURRENT` / `SUPPLY_OUT_OF_RANGE` / `NANO_OFFLINE` / `NANO_FAULT` | A running test was stopped | Fix, `RESET_FAULT` |
| `VALIDATION=FAIL|ERROR=MISSING_<key>` etc. | Module file invalid | Needs `VEHICLE, MODULE, COMM, VOLTAGE_MIN, VOLTAGE_MAX, CURRENT_MAX, TEST_TIMEOUT`; `COMM` = CAN or KLINE |
| `POWER_ON=BLOCKED`, `START_TEST=BLOCKED` | Pretest failed | See the `PRETEST=` reason |

On the V2 **Nano**, a fast-blinking LED means fault (heartbeat lost or e-stop). A
steady LED means an output is on. V2 temperature is a raw voltage, and relay tests and
diagnostics don't exist on V2.

---

## 10. Recovery and factory reset

| What | How |
|---|---|
| Forgotten PIN / Wi-Fi password, or CAN settings gone wrong | Delete `/CONFIG.INI` from the SD card and power-cycle -> defaults (`tifftester`, `TIFF2026`, 500 kbit/s, 8 MHz) |
| Nano calibration and relay polarity | `RESET_CALIBRATION` |
| Nano over-temperature trip / aux auto-off | `SET_TEMP_LIMIT,0` and `SET_AUX_TIMEOUT,0` |
| ESP32 won't boot after a bad flash | Erase flash and re-flash ([FLASHING_GUIDE.md](FLASHING_GUIDE.md) section 10) |
| App state | Settings > Clear app data (results only); uninstall to remove vehicle/module records too |

## 11. Things that look like faults but aren't

- **A BLE disconnect doesn't cut the DUT.** Only losing the ESP32 or a protection trip does.
- **The app can't tell a wrong PIN**, and shows zeros (not `--`) when the Nano is offline.
- **Position reads 0 %** when nothing is wired. **K-LINE READY** doesn't mean an ECU answered.
- **Relay test `ACTUATED`** is not a pass.
- **The display (INA219) and the Nano's protection can disagree**; the Nano's sensors decide.
- **Injector/coil tests report `NOT_IMPLEMENTED`:** no driver hardware yet.

## 12. When you need to report a problem

Collect, so it can be reproduced:

1. The **repo commit** you flashed (`git rev-parse --short HEAD`) and the ESP32/AVR core versions.
2. **Serial output** from the ESP32 (and the Nano's `STATUS` lines if relevant), from boot to the problem.
3. The **fault text** and the SYSTEM line, and the app's Diagnostics console if it is a bus issue.
4. `/LOGS/system.log`, `tests.log`, `DIAG.CSV` and the `TEST_*.CSV` from the SD card.
5. What was connected (DUT or dummy load), supply settings, and a photo of the wiring.
6. The exact steps, and whether it repeats.

# Hardware bring-up checklist (bench)

First power-up and verification of the TIFF TESTER PRO hardware, in the order
that finds problems cheapest and safest. **Nothing in this firmware has been
validated on real hardware yet** - this checklist is how that validation
happens. Print it, tick the boxes, and write the measured values in the log
at the end.

Covers the **main system** (ESP32 BLE firmware + Arduino Nano safety
controller) in Parts 0-8. The separate **V2 Bluetooth Classic** line has its
own short Part 9.

**Stop immediately** (power off, e-stop, unplug) on: smoke or a burning smell,
anything getting hot, a relay clicking when nothing commanded it, a reading
that is wildly wrong, or any behaviour you can't explain. Do not "just try
the DUT" past an unexplained result.

---

## Part 0 - Before you start

**Have on the bench**
- [ ] Current-limited bench supply, set to **12 V**, current limit **1 A** to start
- [ ] Multimeter (trusted), and ideally an oscilloscope or logic analyser
- [ ] Known resistive loads: ~12 Ω (about 1 A) and ~5 Ω (about 2.4 A, 29 W rating or better)
- [ ] A fuse in the supply feed, sized below the wiring rating
- [ ] A working **normally-closed (NC)** e-stop switch, wired to the Nano (see Part 1)
- [ ] USB cables for the Nano and the ESP32, a laptop with the Arduino IDE
- [ ] An Android phone with the app installed (build from `Mobile_App/`)
- [ ] A FAT32 microSD card
- [ ] **No DUT connected.** Dummy loads only until Part 7.

**Know the limits baked into the firmware**
- Supply window **11-15 V**, DUT over-voltage **15 V**, current hard cap **5 A**
- Nano heartbeat timeout **2 s** (the ESP32 sends one every 250 ms)
- Default Wi-Fi AP `TIFF_TESTER` / password `tifftester`; default BLE PIN
  `TIFF2026` - **change both before this leaves the bench**

---

## Part 1 - Unpowered inspection

Build and check against the wiring diagrams: [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md).

Do all of this with **everything unplugged**.

**Ground and power**
- [ ] One common ground between the supply, Nano, ESP32, relay module, current sensor and CAN/K-Line interfaces
- [ ] Supply feed passes through the fuse and the DUT relay contacts; the DUT only ever sees power through relay 1
- [ ] Relay-module coil supply is correct for the module (usually 5 V) and not taken from a Nano I/O pin

**Nano pin map** (continuity check each)
- [ ] D4 relay 1 (DUT), D5 relay 2, D7 relay 3, D8 relay 4
- [ ] D3 e-stop, **fail-safe wiring:** a **normally-closed** contact between D3 and GND. Closed = healthy = D3 LOW. Pressing the e-stop **or any broken/unplugged wire** opens the circuit, the pull-up takes D3 HIGH, and the Nano treats that as an e-stop (`ESTOP_FAILSAFE_NC 1`, the default).
- [ ] **No e-stop fitted?** The Nano will sit in `ESTOP` and refuse everything. Jumper D3 to GND instead of leaving it open. (Building with `ESTOP_FAILSAFE_NC 0` restores the old normally-open behaviour, which does not detect a broken wire - not recommended.)
- [ ] A pressed e-stop and a broken wire look identical to the firmware (both read HIGH). For long or noisy runs, add an external 4.7-10 k pull-up from D3 to 5 V and keep the wire short.
- [ ] D6 buzzer, D13 status LED
- [ ] A0 DUT voltage, A1 supply voltage (both through a divider, default ratio **4:1**, so 20 V full scale on the 5 V ADC)
- [ ] A2 current sensor output (ACS712 5 A assumed: ~2.5 V at zero, ~185 mV/A)
- [ ] A3 temperature sensor - **only if fitted** (LM35 assumed, 10 mV/°C). If nothing is fitted, leave the over-temperature trip off.
- [ ] Optional relay feedback inputs D2 / D11 / D12 - only if you wired them (Part 4)

**ESP32 pin map**
- [ ] SD CS = GPIO5, MCP2515 CS = GPIO15 (INT = GPIO4, wired but unused)
- [ ] K-Line RX = GPIO25, TX = GPIO26 (via the L9637D)
- [ ] INA219: SDA = GPIO21, SCL = GPIO22 (optional)
- [ ] Position input = GPIO33 (0-3.3 V only)
- [ ] ESP32 GPIO16 (RX2) <- Nano TX (D1); ESP32 GPIO17 (TX2) -> Nano RX (D0); GND common

**Voltage levels - the ones that destroy things**
- [ ] **Nano TX (5 V) -> ESP32 RX (GPIO16): the ESP32 is not 5 V tolerant.** Confirm a resistor divider or level shifter is fitted. The wiring doc does not specify one; add it if it is missing.
- [ ] No raw 12 V on any ESP32 or Nano GPIO. Measure with a meter on every divider *before* connecting them.
- [ ] Voltage dividers: with 12 V on the input, the divided output is ~3.0 V (4:1) - measure it, don't assume
- [ ] Position input cannot exceed 3.3 V

**Buses**
- [ ] MCP2515 crystal: **read the number printed on the crystal** (8 MHz or 16 MHz). Write it down: ______ MHz. The default is 8 MHz.
- [ ] CAN bus has 120 Ω termination at **each end** (many MCP2515 boards have a jumper). One end only if you are using a single module at the bench with its own termination - know which case you are in.
- [ ] L9637D: supplied from battery voltage as its datasheet requires; K-Line pulled up (the transceiver or the bus does this)

---

## Part 2 - Flash and first boot

**Nano**
- [ ] Disconnect Nano D0/D1 from the ESP32 (they share the programming UART)
- [ ] Board: Arduino Nano, processor ATmega328P (**Old Bootloader**), as CI builds it
- [ ] Upload `Arduino_Nano_Safety/TIFF_TESTER_PRO_SD_NANO.ino`

**ESP32**
- [ ] Board: ESP32 Dev Module, **Tools > Partition Scheme > Huge APP (3MB No OTA/1MB SPIFFS)** - without it the build fails with "text section exceeds available space"
- [ ] Upload `ESP32_Firmware/` (all tabs in the folder)
- [ ] Insert the SD card **before** power-up
- [ ] Open the serial monitor at 115200. Expect, in order:
  - [ ] `BOOT: setup() start`, `BOOT: Nano UART ready.`
  - [ ] `SD ready.` (if you see `SD init failed.` reformat as FAT32 and check CS wiring)
  - [ ] `CAN: MCP2515 initialized.` (if "not responding": SPI wiring/CS/power)
  - [ ] `INA219: found.` or `INA219: not found.` (optional part - note which: ______)
  - [ ] `SELFTEST CAN loopback: PASS - loopback frame sent and received`
  - [ ] `SELFTEST K-Line idle: PASS - RX idle high` (FAIL = transceiver unpowered or line held low)
  - [ ] `BOOT: Wi-Fi AP started.` then `AP IP: 192.168.4.1`
  - [ ] `BLE: advertising as TiffTester.` and `BOOT: BLE init done.`
- [ ] `/CONFIG.INI` was created on the SD card, and `/MODULES`, `/REPORTS`, `/LOGS` exist
- [ ] `/LOGS/system.log` contains a warning that the default Wi-Fi password / PIN are active (expected)

---

## Part 3 - Nano on its own (ESP32 disconnected)

Connect the Nano by USB only. Open a serial terminal at 115200, **newline** line
ending. The Nano prints `STATUS,...`, `EXT,...` and `LIMITS,...` lines every
500 ms by itself. Supply **12 V** to the supply sense input, no DUT.

**3.1 Boot state**
- [ ] All four relays are **OFF** at power-up (relay LEDs dark, no click). If any relay is ON at boot, the polarity is wrong: send `SET_RELAY_POLARITY,0` for an active-high module (the default is active-low), power-cycle, and re-check. Do not continue until all four are off at boot.
- [ ] With the e-stop circuit closed (the NC contact or a D3-to-GND jumper in place), `STATUS` returns `STATUS,0,0,0,1,<supply>,<dut>,<current>,` (relay 0, fault 0, estop 0, watchdog 1). If you see estop `1` and fault `ESTOP` instead, D3 is open: that is the fail-safe working, so fit the jumper or the e-stop.
- [ ] `EXT,0,0,0,0,<temp>,1` (last field `1` = active-low) and `LIMITS,11.00,15.00,5.00,0.0,0`
- [ ] `GET_CONFIG` returns `CONFIG,4.0000,4.0000,2.5000,0.1850,1,0` (the last `0` = not calibrated yet)

**3.2 E-stop**
- [ ] E-stop circuit closed (not pressed): `STATUS` shows estop `0` and `POWER_ON` works
- [ ] Press the e-stop: `STATUS` shows estop `1` and fault text `ESTOP`; buzzer sounds
- [ ] With it pressed, `POWER_ON` does **not** energise relay 1
- [ ] Release it and send `RESET_FAULT`: fault clears, buzzer stops
- [ ] **Wire-break test:** with relay 1 on (keep sending `HEARTBEAT`), disconnect one e-stop wire: relay 1 and every aux relay drop at once, fault `ESTOP`, buzzer sounds. Reconnect and `RESET_FAULT`.
- [ ] **Open at boot:** power-cycle the Nano with the e-stop wire disconnected: it comes up already faulted `ESTOP` with every relay off.

**3.3 Calibration** (uses a trusted meter; the defaults are placeholders)
- [ ] Measure the supply at the divider input with the meter: ______ V; send `CAL_SUPPLY,<that value>`
- [ ] Send `POWER_ON` and measure the DUT output node: ______ V; send `CAL_DUT,<value>` (relay must be on so there is voltage to read)
- [ ] `POWER_OFF`. With **no load**, `CAL_CURRENT_ZERO`
- [ ] Put the ~12 Ω load on, `POWER_ON`, measure current with a trusted ammeter: ______ A; send `CAL_CURRENT_SCALE,<amps>`
- [ ] `GET_CONFIG` now ends `,1` (calibrated). Record: supply div ______ DUT div ______ zero ______ V/A ______
- [ ] Re-check at a second load: the Nano's reading agrees with the meter within your tolerance: ______ %

**3.4 Protection trips** (the Nano must cut the relay by itself)
- [ ] Supply below 11 V (set the bench supply to 10.5 V): `POWER_ON` is refused, fault `UNDERVOLTAGE`
- [ ] Supply above 15 V (set 15.5 V, **supply current limit still on**): refused, fault `OVERVOLTAGE`
- [ ] Return to 12 V, `RESET_FAULT`
- [ ] **Heartbeat dead-man:** send `POWER_ON` and send nothing else: within about 2 s relay 1 drops with fault `ESP32_HEARTBEAT_TIMEOUT`. (Sending `HEARTBEAT` once a second keeps it on.)
- [ ] **Over-current without a dangerous load:** send `SET_LIMITS,11,15,1.0`, then `POWER_ON` into the ~5 Ω load (about 2.4 A): trips `OVERCURRENT`. Then `RESET_LIMITS` and `RESET_FAULT`.
- [ ] `SET_LIMITS,5,20,9` is **clamped**: `LIMITS` reports `11.00,15.00,5.00` (limits can only tighten)
- [ ] Over-temperature (only if a temperature sensor is fitted): `EXT` shows a sensible room temperature ______ °C; `SET_TEMP_LIMIT,30` with a warm sensor trips `OVERTEMPERATURE`; then `SET_TEMP_LIMIT,0`

**3.5 Relays 2-4**  (send `HEARTBEAT` at least every 2 s while testing - aux relays also drop on heartbeat loss)
- [ ] `RELAY,2,1` / `RELAY,2,0` switches relay 2; same for 3 and 4; `EXT` reflects each
- [ ] `RELAY_TEST,2` produces `RUNNING` lines then `DONE,3,ACTUATED,` in about 7 s (three audible clicks). `ACTUATED` is not a pass - you confirm the clicks.
- [ ] `RELAY_TEST,1` (needs the DUT output node connected with some load): `PASS`, or `FAIL` with `NO_DUT_VOLTAGE_WHEN_ON` if nothing is connected - record: ______
- [ ] `RELAY_PULSE,2,1000` energises relay 2 for about 1 s then releases by itself
- [ ] `SET_AUX_TIMEOUT,5`, then `RELAY,2,1` (keep heartbeating): relay 2 drops after about 5 s. `SET_AUX_TIMEOUT,0` afterwards.
- [ ] Press the e-stop with a relay on: **every** relay drops
- [ ] If you wired relay feedback inputs: set `AUX_FEEDBACK_ENABLED` to 1, re-flash, and `RELAY_TEST,2` now reports `PASS`; unplugging the sense wire makes it `FAIL`

**3.6 Reset behaviour**
- [ ] With a relay on, briefly remove and restore the Nano's 5 V (a power reset): it comes back up with all four relays **off** and does not re-energise anything by itself. (The hardware watchdog that resets a hung Nano cannot be exercised without modifying the sketch, so it is not part of this checklist.)

---

## Part 4 - ESP32 and Nano together

Reconnect D0/D1 <-> GPIO16/17 (with the level shifting from Part 1).

- [ ] Serial monitor shows `STATUS`-driven updates and no repeated errors
- [ ] **Kill the ESP32 while relay 1 is energised:** with the supply at 12 V, switch the DUT on from the app or the web UI (`/api/power/on`), then reset or unplug the ESP32. Relay 1 must drop within ~2 s (the Nano's fault text becomes `ESP32_HEARTBEAT_TIMEOUT`). This is the core dead-man test of the whole design. (Don't use the Nano's USB terminal for this: its UART is the link to the ESP32.)
- [ ] From the web UI (join Wi-Fi `TIFF_TESTER`, open `http://192.168.4.1/`): the Live Data block updates every second and matches the Nano's values
- [ ] `http://192.168.4.1/api/live` shows plain-text live data in the agreed layout (supply/DUT voltage, current, position, temperature, relays, CAN/K-LINE/INA219/SYSTEM)
- [ ] The web relay buttons switch relays 1-4; TEST buttons run the relay tests

---

## Part 5 - ESP32 peripherals

**5.1 SD card and modules**
- [ ] **Module file location:** the firmware lists and loads modules from `/MODULES/<id>.INI` - **top level only, no subfolders**. The example in the repo sits at `SD_MODULES/MODULES/TOYOTA/HILUX_1KD_TURBO.INI`; copy it to the card as `/MODULES/TOYOTA_HILUX_1KD_TURBO.INI` (matching the `id=` inside) or it will not be found.
- [ ] `/api/modules` (or the app's module list) shows the module; selecting it works
- [ ] `/api/module/active` shows the module's limits; the Nano's `LIMITS` line reflects them (within 5 s of selecting)

**5.2 INA219 (optional)**
- [ ] With a ~12 Ω load energised, INA219 current agrees with the meter: ______ A vs ______ A (range tops out near 3.2 A with a 0.1 Ω shunt)
- [ ] INA219 bus voltage agrees with the DUT-node voltage: ______ V vs ______ V
- [ ] Unplug the INA219: status goes to `NOT FOUND` and recovers when re-plugged (retry every 5 s)

**5.3 Position input (GPIO33)**
- [ ] Unconnected reads `0 %` - that is the internal pull-down, **not a real position**
- [ ] Feed 0 V, 1.65 V, 3.3 V from a divider: reads about 0 / 50 / 100 %

**5.4 CAN** (needs a USB-CAN analyser or a second CAN node)
- [ ] Set the correct crystal and speed in the app (Diagnostics > CAN > CAN INIT) or `can_clock_mhz=` in `/CONFIG.INI`. Record: ______ MHz, ______ bit/s
- [ ] `SELFTEST` -> CAN loopback PASS (this proves the controller, not the transceiver or wiring)
- [ ] Analyser sends frames: the app's CAN monitor shows them with the right IDs and data
- [ ] App sends a raw frame: the analyser receives it with the right ID/data at the right bit rate
- [ ] Deliberately wrong speed on one side: errors/no traffic (confirms the timing setting matters)
- [ ] ISO-TP/UDS against a real ECU or a simulator **only after** the above. If requests time out with correct IDs, try the other ISO-TP padding options. Real CAN IDs for each module are not verified in this repo - a warning is printed when generic defaults are in use.

**5.5 K-Line**
- [ ] Idle level high on a scope; `SELFTEST` -> K-Line idle PASS
- [ ] `SELFTEST:KLINE_ECHO` (briefly pulls the line low; not while an ECU is talking): PASS
- [ ] Fast-init and 5-baud init only against a real ECU or simulator, **with a scope on the line** - timing is per the standards but unvalidated

---

## Part 6 - Phone app over BLE

- [ ] Phone Bluetooth and location permissions granted; the app scans and finds `TiffTester`
- [ ] A wrong PIN does not give control (relays do not respond to the app); five wrong PINs trigger the lockout/backoff
- [ ] **Change the default PIN** (Settings) - the red warning banner disappears
- [ ] Live Data matches the Nano and the meter; supply/DUT/current/position/temperature all plausible; CAN/K-LINE show `READY ✓`
- [ ] DUT ON / DUT OFF work; **ALL OUTPUTS OFF** drops everything immediately
- [ ] Controls: relay switches and TEST work for relays 2-4; the **HOLD** button energises a relay only while pressed (release it and also try force-closing the app while holding: the relay must drop within about a second)
- [ ] Controls > Protection: current limit shows `5.0 A` with no module selected, and the module's limit after selecting one; temperature trip and aux auto-off settings stick after a power-cycle
- [ ] Relay polarity card shows `ACTIVE-LOW` and matches what you measured in 3.1
- [ ] Run a quick test, then check Results and **export the PDF** (it includes the live-data table once you have been connected a while)
- [ ] Live Graph fills and the CSV copy pastes cleanly into a spreadsheet
- [ ] Diagnostics > Self-test runs and prints PASS/FAIL lines

---

## Part 7 - Safety behaviour with a dummy load, then the DUT

Do these with the **~5 Ω dummy load first**. Only then the real module, with the
bench supply current limit set to about twice the module's expected current.

- [ ] E-stop while DUT is on: relay 1 and every aux relay drop; app shows fault; `ALL OUTPUTS OFF` still works
- [ ] After an e-stop, power does not come back on its own: it needs the e-stop released **and** `RESET_FAULT`
- [ ] Supply sagging below 11 V (turn the bench supply down) while DUT is on: relay drops with `UNDERVOLTAGE`
- [ ] ESP32 power removed while DUT is on: relay drops within ~2 s
- [ ] Close the app / walk the phone out of range while DUT is on: **record what happens: ______.** Expected today: the DUT stays ON - the ESP32 keeps sending heartbeats and does **not** treat a BLE disconnect as a stop. Decide whether you accept that for your bench before using a real module unattended.
- [ ] With the real module: select its profile (limits load into the Nano), power on at the bench-supply current limit, watch current and temperature on the Live Data card
- [ ] Module draws no more than its profile's `max_current_a`; if it trips, stop and investigate rather than raising limits

---

## Known gaps this checklist exercises (not fixed in firmware)

Decide how you will handle each before relying on the bench:

- **E-stop cannot tell "pressed" from "wire broken"** - both read as an e-stop
  (that is the fail-safe design). A board with no e-stop fitted needs D3
  jumpered to GND.
- **A BLE disconnect does not turn the DUT off.** Only loss of the ESP32 itself
  (heartbeat) or a protection trip does.
- **Nano TX to ESP32 RX voltage level** is not specified in the wiring doc; the
  ESP32 is not 5 V tolerant.
- **Module files must sit at the top level of `/MODULES`** on the SD card; the
  repo's example is in a subfolder.
- **Relay polarity default changed to active-low**; a Nano with a polarity
  already saved in EEPROM keeps it. Always confirm at boot (3.1).
- **Over-temperature protection is off by default** and meaningless without a
  sensor on A3.
- **V2 line:** its test-time safety checks only run during a started test.

---

## Part 8 - Sign-off and record

Before you call the bench "up":
- [ ] Wi-Fi password and BLE PIN changed from the defaults
- [ ] Calibration values recorded below and the Nano reports calibrated
- [ ] All Part 3 trips and the Part 4 dead-man test passed
- [ ] The e-stop is the one you intend to use, is the right contact type, and has been tested in its final position
- [ ] Anything that failed or surprised you is written down, not forgotten

**Log**

| Item | Value |
|---|---|
| Date / who | |
| Nano sketch commit | |
| ESP32 sketch commit | |
| App build | |
| MCP2515 crystal | MHz |
| Supply divider / DUT divider | |
| Current zero (V) / scale (V per A) | |
| Relay polarity as measured | |
| INA219 present / shunt | |
| Temperature sensor fitted | |
| CAN speed used | |
| Failures / notes | |

---

## Part 9 - V2 Bluetooth Classic line (only if you use it)

Separate hardware path: `ESP32_Firmware_V2/` with the I2C Nano in
`Arduino_Nano_V2/`. See [../ESP32_Firmware_V2/README.md](../ESP32_Firmware_V2/README.md).

- [ ] **I2C levels:** ESP32 is 3.3 V, Nano is 5 V. Fit a bidirectional level shifter (or 3.3 V pull-ups). The Nano V2 sketch turns its internal pull-ups off. Never connect SDA/SCL directly with 5 V pull-ups.
- [ ] Nano V2 pins: relays D4/D5/D7/D8 (active-low), MOSFETs D9/D10 (active-high), e-stop D3 (normally-closed contact to GND; jumper it if none fitted), I2C on A4/A5. All outputs are **off at boot**.
- [ ] I2C address 0x12 shows up on an I2C scan
- [ ] ESP32 V2 serial boot: `Bluetooth: READY`, `INA219: READY`, `SD Card: READY`, `Nano: ONLINE`
- [ ] Pair `TIFF_TESTER_V2` in Android Bluetooth settings (no PIN), then connect from the app
- [ ] `RELAY2_ON` replies `OK|RELAY2=ON|CONFIRMED`; `STATUS` shows `R2=ON`
- [ ] Unplug the Nano's I2C: `NANO_OFFLINE`, status shows relays `UNKNOWN`
- [ ] Press the Nano e-stop (or disconnect its wire): ON commands come back `NOT_CONFIRMED`, a running test faults with `NANO_FAULT`
- [ ] Stop the ESP32 while a Nano relay is on: it drops after about 1.5 s
- [ ] `VALIDATE_MODULE` needs the V2 module format (flat `KEY=VALUE`, e.g. `COMM`, `CAN_SPEED`), **not** the main firmware's `[SECTION]` format
- [ ] V2 temperature is a raw ADC voltage, not °C (the app shows n/a)

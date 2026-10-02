# Firmware flashing guide

How to build and flash every piece of TIFF TESTER PRO: the Arduino Nano, the
ESP32, and the Android app. Settings here match what CI compiles (see
`.github/workflows/ci.yml`); a wrong board option is the most common reason a
build or upload fails.

Do this **before** the bench bring-up in [BRINGUP_CHECKLIST.md](BRINGUP_CHECKLIST.md),
with wiring per [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md).

## 0. What you are flashing

Pick **one** build. They are different hardware paths and do not interoperate.

| Build | Microcontroller | Sketch | Board settings |
|---|---|---|---|
| **Main system** | Arduino Nano | `Arduino_Nano_Safety/TIFF_TESTER_PRO_SD_NANO.ino` | Arduino Nano, ATmega328P **(Old Bootloader)** |
| | ESP32 | `ESP32_Firmware/` (main `.ino` + tab `.ino` files + `shared_types.h`) | ESP32 Dev Module, **Partition Scheme: Huge APP** |
| **V2 line** | Arduino Nano | `Arduino_Nano_V2/TIFF_TESTER_PRO_V2_NANO.ino` | Arduino Nano, ATmega328P **(Old Bootloader)** |
| | ESP32 | `ESP32_Firmware_V2/TIFF_TESTER_PRO_V2_ESP32.ino` | ESP32 Dev Module, **Partition Scheme: Huge APP** |
| Either | Android phone | `Mobile_App/` | Android 8.0+ (minSdk 26) |

The app talks BLE to the main system and Bluetooth Classic to V2.

## 1. Before you plug anything in

- [ ] **Remove the DUT, the aux loads and the 12 V feed.** While a Nano is being
      programmed its pins float, and relay modules can click or glitch. Flash with
      only USB connected.
- [ ] **Disconnect the Nano <-> ESP32 UART** (main system: Nano D0/D1 to ESP32
      GPIO16/17). Nano D0/D1 are shared with USB, so an attached ESP32 breaks
      Nano uploads.
- [ ] USB drivers: cheap Nano/ESP32 clones usually use a CH340 or CP210x USB chip.
      If no COM port appears, install that chip's driver. A **charge-only** cable
      also shows no port - try another cable.
- [ ] Work on one board at a time so you know which COM port is which.

## 2. Toolchain

Either the Arduino IDE 2.x or `arduino-cli`. Both need the same two board
packages.

**Arduino IDE 2.x**
1. File > Preferences > *Additional boards manager URLs*, add:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   (the same URL CI uses)
2. Boards Manager: install **Arduino AVR Boards** (for the Nano) and **esp32 by
   Espressif Systems**.
3. **V2 only:** Library Manager: install **Adafruit INA219** (it pulls in Adafruit
   BusIO). The main firmware needs no extra libraries.

**arduino-cli** (PowerShell)
```powershell
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit INA219"     # V2 only
```

**ESP32 core version.** The V2 sketch uses `esp_task_wdt_config_t`, which needs
**ESP32 core 3.x**. CI builds against whatever the latest esp32 core is at build
time (it is not pinned), so if a future core release breaks the build, install the
last version that worked and note it in your flash log (section 10).

## 3. Put each sketch in a correctly named folder

Arduino requires a sketch's **folder name to match its main `.ino` name**. The repo
organises firmware by role instead, so copy each one into a matching folder first
(this is exactly what CI does). From the repo root, in PowerShell:

```powershell
$out = "$HOME\Documents\Arduino"

# Main system
New-Item -ItemType Directory -Force "$out\TIFF_TESTER_PRO_SD_NANO" | Out-Null
Copy-Item Arduino_Nano_Safety\TIFF_TESTER_PRO_SD_NANO.ino "$out\TIFF_TESTER_PRO_SD_NANO\"

New-Item -ItemType Directory -Force "$out\TIFF_TESTER_PRO_SD_ESP32" | Out-Null
Copy-Item ESP32_Firmware\*.ino, ESP32_Firmware\*.h "$out\TIFF_TESTER_PRO_SD_ESP32\"

# V2 line
New-Item -ItemType Directory -Force "$out\TIFF_TESTER_PRO_V2_NANO" | Out-Null
Copy-Item Arduino_Nano_V2\TIFF_TESTER_PRO_V2_NANO.ino "$out\TIFF_TESTER_PRO_V2_NANO\"

New-Item -ItemType Directory -Force "$out\TIFF_TESTER_PRO_V2_ESP32" | Out-Null
Copy-Item ESP32_Firmware_V2\TIFF_TESTER_PRO_V2_ESP32.ino "$out\TIFF_TESTER_PRO_V2_ESP32\"
```

The main ESP32 sketch is **several files** (`TIFF_TESTER_PRO_SD_ESP32.ino` plus
`ble_service.ino`, `can_mcp2515.ino`, `config.ino`, `diag_engine.ino`,
`ina219.ino`, `kline_iso14230.ino`, `live_data.ino`, `test_orchestrator.ino`,
`uds_isotp.ino` and `shared_types.h`). Copy **all** of them or the build fails with
undefined references. Re-copy after every `git pull`.

## 4. Flash order

1. **Nano first**, on its own, with the UART link disconnected.
2. **ESP32 next**, on its own.
3. Only then reconnect the UART (and, for V2, the I2C level shifter) and power the
   assembly - still without the DUT.
4. Android app any time.

## 5. Main system - Arduino Nano

**Board settings (IDE):** Tools > Board > *Arduino Nano*; Processor *ATmega328P
(Old Bootloader)*; Port: the Nano's COM port.

**arduino-cli:**
```powershell
$fqbn = "arduino:avr:nano:cpu=atmega328old"
arduino-cli compile --fqbn $fqbn "$HOME\Documents\Arduino\TIFF_TESTER_PRO_SD_NANO"
arduino-cli board list                                    # find the COM port
arduino-cli upload -p COM5 --fqbn $fqbn "$HOME\Documents\Arduino\TIFF_TESTER_PRO_SD_NANO"
```

**If the upload fails** with `stk500_getsync(): not in sync`: switch the processor
option between *Old Bootloader* and *ATmega328P* (genuine newer Nanos use the new
bootloader; most clones need the old one), check the port, and unplug anything on
D0/D1.

**Verify:** open the serial monitor at **115200**. With nothing else connected the
Nano prints `STATUS,...`, `EXT,...` and `LIMITS,...` lines every 500 ms. Typing
`GET_CONFIG` (newline-terminated) returns a `CONFIG,...` line. If this works the
Nano is flashed; the real checks are in the bring-up checklist, Part 3.

**Compile-time options to review before you upload** (edit the sketch, then flash):

| Setting | Default | Change it when |
|---|---|---|
| `ESTOP_FAILSAFE_NC` | `1` | `1` = normally-closed e-stop to GND (fail-safe on a broken wire; jumper D3 to GND if none fitted). `0` = old normally-open behaviour, which does **not** detect a broken wire. |
| `AUX_FEEDBACK_ENABLED` | `0` | `1` only if you wired relay-feedback inputs on D2/D11/D12 |
| `TEMP_C_PER_V` | `100.0` | your temperature sensor is not an LM35 (10 mV/°C) |
| `RELAY_DEFAULT_ACTIVE_LOW` | `true` | your relay module is active-HIGH and you do not want to set it at runtime |

Relay polarity, calibration and the over-temperature / aux-timeout settings are
changed **at runtime** and stored in EEPROM, so they do not need a re-flash.

## 6. Main system - ESP32

**Board settings (IDE):** Tools > Board > *ESP32 Dev Module*; **Tools > Partition
Scheme > Huge APP (3MB No OTA/1MB SPIFFS)**; Upload Speed 921600 (drop to 115200 if
uploads are flaky); Port: the ESP32's COM port.

> **The Partition Scheme is mandatory.** Wi-Fi + web server + SD + BLE + CAN +
> K-Line do not fit the default partition. Without it the build stops with *"text
> section exceeds available space in board"*.

**arduino-cli:**
```powershell
$fqbn = "esp32:esp32:esp32:PartitionScheme=huge_app"
arduino-cli compile --fqbn $fqbn "$HOME\Documents\Arduino\TIFF_TESTER_PRO_SD_ESP32"
arduino-cli upload -p COM6 --fqbn $fqbn "$HOME\Documents\Arduino\TIFF_TESTER_PRO_SD_ESP32"
```

**Prepare the SD card first:** FAT32, inserted before power-up. The firmware
creates `/MODULES`, `/REPORTS`, `/LOGS` and `/CONFIG.INI` itself. Module files must
sit at the **top level** of `/MODULES` as `<id>.INI` (no subfolders); copy the
example from `SD_MODULES/MODULES/TOYOTA/HILUX_1KD_TURBO.INI` to
`/MODULES/TOYOTA_HILUX_1KD_TURBO.INI`.

**If the upload will not start** (`Failed to connect`, `Wrong boot mode`): hold the
**BOOT** button while it says *Connecting...*, then release. If it still fails,
disconnect whatever is on GPIO2/5/15 (LED, SD CS, MCP2515 CS - boot-strapping pins)
and retry.

**Verify:** serial monitor at **115200**, reset the board. Expect, in order,
`BOOT: setup() start`, `SD ready.`, `CAN: MCP2515 initialized.`,
`SELFTEST CAN loopback: PASS ...`, `BOOT: Wi-Fi AP started.`, `AP IP: 192.168.4.1`,
and `BLE: advertising as TiffTester.` Detailed expectations are in the bring-up
checklist, Part 2.

**Settings that live on the SD card, not in the firmware:** `/CONFIG.INI` holds the
Wi-Fi password, BLE PIN, and `can_bitrate` / `can_clock_mhz`. Set the CAN crystal
(8 or 16 MHz, read it off the MCP2515 board) from the app's Diagnostics > CAN INIT
or by editing `can_clock_mhz=` - no re-flash needed.

## 7. V2 line

**Nano V2** - flash exactly as in section 5, using
`Arduino_Nano_V2/TIFF_TESTER_PRO_V2_NANO.ino`. Review `ENABLE_ESTOP`,
`ESTOP_FAILSAFE_NC` (same meaning as above), `RELAY_ACTIVE_LOW` and `I2C_ADDRESS`
(must stay `0x12` to match the ESP32 sketch).

**ESP32 V2** - same board settings as section 6 (**Huge APP**), plus the **Adafruit
INA219** library and **ESP32 core 3.x**:
```powershell
$fqbn = "esp32:esp32:esp32:PartitionScheme=huge_app"
arduino-cli compile --fqbn $fqbn "$HOME\Documents\Arduino\TIFF_TESTER_PRO_V2_ESP32"
arduino-cli upload -p COM6 --fqbn $fqbn "$HOME\Documents\Arduino\TIFF_TESTER_PRO_V2_ESP32"
```
Verify at 115200: `Bluetooth: READY`, `INA219: READY`, `SD Card: READY`, then
`Nano: ONLINE` once the Nano and level shifter are connected. Pair `TIFF_TESTER_V2`
in Android's Bluetooth settings (no PIN) before connecting from the app.

## 8. Android app

Needs the Flutter SDK (stable; this repo was last built with Flutter 3.47) and the
Android toolchain. From `Mobile_App/`:

```powershell
flutter pub get
flutter build apk --release        # or --debug, which is what CI builds
adb install -r build\app\outputs\flutter-apk\app-release.apk
```
or `flutter run` with a phone attached over USB (USB debugging on).

- BLE does not work in the emulator - use a **physical Android 8.0+ phone**.
- On first launch grant the Bluetooth ("nearby devices") and location permissions;
  the app asks before scanning.
- **The release build is signed with the debug key** (`android/app/build.gradle.kts`
  uses the `debug` signing config). That is fine for your own bench phone; do not
  distribute it, and add a real signing config before sharing the APK.
- Installing a build signed with a different key over an existing one fails: uninstall
  the old app first (your vehicle/module records and results are stored on the phone
  and will be lost).

## 9. First-boot configuration

After flashing and the first successful connection, before relying on the bench:

- [ ] **Change the default BLE PIN** (`TIFF2026`): app Settings, or the web API.
- [ ] **Change the default Wi-Fi password** (`tifftester`): join `TIFF_TESTER` and
      call the web API, then **reboot** the ESP32 so the AP picks it up:
      ```powershell
      Invoke-RestMethod -Method Post http://192.168.4.1/api/config/wifi -Body @{ current_password="tifftester"; new_password="your-new-password" }
      Invoke-RestMethod -Method Post http://192.168.4.1/api/config/pin  -Body @{ current_pin="TIFF2026"; new_pin="your-new-pin" }
      ```
      (Wi-Fi password: at least 8 characters. PIN: at least 6.)
- [ ] Confirm the **relay polarity** matches your module (bring-up checklist 3.1).
- [ ] **Calibrate** the Nano (checklist 3.3) - the shipped divider and current-sensor
      values are placeholders.
- [ ] Set the **CAN crystal** if the board is not 8 MHz.

## 10. Updating and re-flashing

What survives a re-flash, and what does not:

| Item | Survives re-flash? |
|---|---|
| Nano calibration, relay polarity, temperature limit, aux timeout (EEPROM) | **Yes** with a normal USB/bootloader upload (programming over ISP can erase EEPROM). A *saved* polarity overrides a changed default. |
| ESP32 `/CONFIG.INI`, `/MODULES`, `/REPORTS`, `/LOGS` (SD card) | **Yes** - on the card |
| App data (vehicles, modules, results) | **Yes**, unless you uninstall the app |
| Module protection limits on the Nano | No - RAM only; the ESP32 re-sends them within 5 s |

**Resetting to defaults**
- *Forgot the PIN or Wi-Fi password:* delete `/CONFIG.INI` from the SD card and
  power-cycle. The firmware recreates it with the defaults (`tifftester` /
  `TIFF2026`, 500 kbit/s, 8 MHz) and logs a warning.
- *Nano calibration and polarity:* send `RESET_CALIBRATION`. Also send
  `SET_TEMP_LIMIT,0` and `SET_AUX_TIMEOUT,0` to clear those two settings.
- *ESP32 won't boot after a bad flash:* erase it and re-flash - IDE 2.x has Tools >
  *Erase All Flash Before Sketch Upload*, or `esptool.py erase_flash`.

**Write down what you flashed** (it is not embedded in the firmware): the repo
commit (`git rev-parse --short HEAD`), the esp32 and AVR core versions
(`arduino-cli core list`), and the date. Put them in the log table at the end of
the bring-up checklist so a later problem can be tied to an exact build.

## 11. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Build: `text section exceeds available space in board` | Wrong partition scheme | Set **Huge APP** (section 6) |
| Build: undefined reference / `... was not declared` in the main ESP32 sketch | Not all tab files copied | Copy every `*.ino` and `shared_types.h` (section 3) |
| IDE: "sketch folder name must match" | Opened the repo file directly | Use the staged folder (section 3) |
| V2 build: `Adafruit_INA219.h: No such file` | Library missing | Install **Adafruit INA219** |
| V2 build: `esp_task_wdt_config_t` unknown | ESP32 core 2.x | Install ESP32 core 3.x |
| Nano upload: `stk500_getsync` | Wrong bootloader option, UART link attached, wrong port | Toggle *Old Bootloader*; unplug D0/D1; check port |
| ESP32 upload: `Failed to connect` | Not in download mode, or a peripheral holds a strapping pin | Hold BOOT; disconnect GPIO2/5/15 loads |
| No COM port | Charge-only cable or missing CH340/CP210x driver | Another cable; install the driver |
| ESP32 boot log: `SD init failed.` | Card not FAT32, wiring, or inserted after power-up | Reformat FAT32; check GPIO5 wiring; re-insert and reset |
| ESP32 boot log: `CAN: MCP2515 not responding` | SPI/CS wiring or module power | Check GPIO15/18/19/23 and the module's supply |
| `SELFTEST K-Line idle: FAIL` | L9637D unpowered or line held low | Check VBAT/VCC; disconnect the DUT and retry |
| App cannot find `TiffTester` | Bluetooth/location permissions, or the V2 firmware is flashed | Grant permissions; V2 is paired in Android settings, not scanned |
| Nano `STATUS` shows `ESTOP` right after boot | D3 open (the fail-safe e-stop) | Fit the NC e-stop, or jumper D3 to GND |

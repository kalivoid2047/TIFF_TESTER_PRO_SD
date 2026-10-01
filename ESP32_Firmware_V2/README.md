# ESP32_Firmware_V2 — Bluetooth Classic "V2" firmware

`TIFF_TESTER_PRO_V2_ESP32.ino` (V2.2) is the **separate Bluetooth-Classic-only
firmware line** for the ESP32-WROOM-DA. It is added to the repo unmodified
(byte-identical to the file it came from). It is *not* the main firmware in
[`../ESP32_Firmware`](../ESP32_Firmware) and the two do not share code or
wire protocol.

| | Main firmware (`ESP32_Firmware/`) | V2 (`ESP32_Firmware_V2/`) |
|---|---|---|
| Link | BLE GATT (PIN) + Wi-Fi AP | Bluetooth Classic SPP (`TIFF_TESTER_V2`), no PIN |
| Nano | UART, text protocol, independent safety MCU | **I2C slave at 0x12**, binary commands |
| Relays | DUT relay + aux relays 2–4 on the Nano, with test + polarity control | DUT relay on ESP32 GPIO27 + Nano relays 1–4 and MOSFETs 1–2 |
| Diagnostics | CAN/ISO-TP/UDS, K-Line/KWP (read-only) | MCP2515 reset check and K-Line UART only |
| Module files | `[SECTION]` INI | flat `KEY=VALUE`, must pass `VALIDATE_MODULE` |

## Build

- Board: ESP32 (core 3.x — it uses `esp_task_wdt_config_t`), with Bluetooth Classic enabled.
- Library: **Adafruit INA219** (installs Adafruit BusIO).
- CI compiles it (`compile-esp32-v2` in `.github/workflows/ci.yml`).

## Wiring assumptions (from the sketch header)

12 V bench; GPIO34 gets a *protected* divider signal (never raw 12 V); INA219
at 0x40; Nano at I2C 0x12; Nano relay inputs active-LOW, MOSFETs active-HIGH;
MCP2515 defaults to an 8 MHz crystal (edit the sketch for 16 MHz). Pins:
I2C 21/22, SPI 18/19/23, MCP2515 CS 5, SD CS 13, K-Line RX/TX 16/17, supply ADC
34, position ADC 36, temperature ADC 39, DUT relay 27.

## Text protocol (newline-terminated; replies use `|` separators)

`STATUS`/`READ_LIVE_DATA`, `LIST_MODULES`, `READ_MODULE|f`, `ADD_MODULE|f|K=V|…`,
`DELETE_MODULE|f`, `SELECT_MODULE|f`, `VALIDATE_MODULE|f`, `VALIDATE_ALL_MODULES`,
`PRETEST`, `START_TEST`, `END_TEST`, `POWER_ON`, `POWER_OFF`, `ALL_OFF`,
`RESET_FAULT`, `RELAY1_ON`…`RELAY4_OFF`, `MOSFET1_ON`…`MOSFET2_OFF`, `SERIAL_LIVE`.

`STATUS` returns one line:
`VOLTAGE=…,DUT_VOLTAGE=…,CURRENT=…,POWER=…,POSITION=…,TEMP=…,DUT=ON|OFF,FAULT=…,NANO=…,INA219=…,SD=…,CAN=…,KLINE=…,WATCHDOG=OK,MODULE=…,STATUS=TESTING|IDLE`.

## Nano I2C protocol (the V2 Nano sketch is **not in this repo**)

The ESP32 writes one command byte to address 0x12: `0x01` get status (reads 1
byte back; `0xFF` = no answer), `0x02` all off, `0x03` heartbeat, `0x10/0x11`
R1 on/off … `0x16/0x17` R4, `0x20/0x21` M1, `0x22/0x23` M2. A matching Nano
sketch would have to implement these; the ESP32 faults with `NANO_OFFLINE` if
it stops answering.

## Things to know about this firmware (observations, nothing changed)

- **`TEMP` is not degrees C.** `readTemperatureC()` is a placeholder that
  returns the raw ADC voltage (see the comment in the sketch). The app shows
  temperature as n/a for V2 for that reason.
- **Relays 1–4 are command-only.** `STATUS` does not report their state, so the
  app shows the last command sent, not a measured state.
- **The DUT relay is active-HIGH** on GPIO27 (`HIGH` = on), while the header's
  "relay inputs are active LOW" refers to the Nano's relay outputs. The
  separate V2.3 notes asked for an active-low DUT relay; this V2.2 file is not
  that version.
- **Safety checks only run during a test.** Over-current, supply-range and
  Nano-online checks (`safetyCheck()`) return immediately unless `START_TEST`
  is active, so a bare `POWER_ON` or a manual relay command is not
  supervised by them.
- Module-file keys differ from the main firmware's schema (`COMM`, `CAN_SPEED`,
  `CAN_TX`/`CAN_RX` as decimal, `VOLTAGE_MIN`, …); the INI parser is a flat
  substring search, so commented lines are not ignored.
- Not validated against real hardware.

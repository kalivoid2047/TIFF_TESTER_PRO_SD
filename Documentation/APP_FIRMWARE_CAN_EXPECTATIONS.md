# What the App Expects from the Firmware (incl. CAN Live Messages)

The app never touches the CAN bus. It talks to the ESP32 over Bluetooth, and
the ESP32 owns the MCP2515/CAN side. The app expects **text and JSON**, not raw
CAN frames. What it expects depends on which firmware it is connected to.

Sources: `Mobile_App/lib/ble/tiff_ble_service.dart`,
`Mobile_App/lib/ble/tiff_classic_link.dart`,
`Mobile_App/lib/models/{nano_status,diag_line,test_result}.dart`,
`ESP32_Firmware/diag_engine.ino`, `ESP32_Firmware/ble_service.ino`,
`ESP32_Firmware_V2/`.

## 1. Minimum needed to start tests (any firmware)

1. **A Bluetooth link.** Test and control screens are gated only on
   `app.isConnected`.
2. **A live status stream.** Without it the app shows zeros / "unknown".
   `canReady` is display-only; it is not a precondition for starting a test.
3. **Replies in the exact formats below.** Malformed messages are silently
   dropped (BLE status parse errors are swallowed; unrecognised V2 lines are
   ignored).

## 2. BLE firmware (`TiffTester`)

### 2.1 Auth
```
APP -> Auth char (6e400002): TIFF2026        (default PIN)
```
No reply is sent. Until the PIN is accepted the firmware ignores every command
except `POWER_OFF`, `STOP_TEST` and `DIAG_STOP`.

### 2.2 Status characteristic (6e400003): JSON every ~250-500 ms
```json
{"relay":0,"fault":0,"estop":0,"watchdog":0,
 "supply_v":12.4,"dut_v":0.0,"current_a":0.0,
 "relays":[0,0,0,0],"temp_c":24,"pos_pct":0,
 "can":1,"kline":1,"ina":1,"sys":1,
 "pol":1,"lim_a":5,"lim_t":0,"aux_to":0,
 "can_st":1,"kline_st":1}
```
- `can` / `kline` / `ina` / `sys`: 0/1 ready flags. `can:1` shows CAN READY.
- `can_st` / `kline_st`: self-test result. -1 not run, 0 failed, 1 passed.
- Missing keys fall back to defaults, so partial JSON is tolerated.

### 2.3 Result characteristic (6e400005): two kinds of text line

**CAN / diagnostics lines** (go to the Diagnostics console):
```
diag:<kind>,<OK|ERR>,<text>
```
- Must start with `diag:` and contain two commas.
- `kind` = text up to the first comma; `OK`/`ERR` sits between the first and
  second comma; everything after the second comma is shown as-is
  (green for OK, red for ERR).
- Max 170 characters (BLE MTU).
- Kinds emitted today: `can_rx`, `can_init`, `can_cfg`, `can_tx`, `uds`,
  `dtc`, `kline`, `kline_rx`, `kwp`, `selftest`, `diag`.

**Test results** (go to the Results screen, no `diag:` prefix):
```
testType,PASS|FAIL,response
```
At least two commas are needed, otherwise the app records `MALFORMED_RESULT` as
a FAIL. The response may itself contain commas.

### 2.4 Example CAN session

`->` is app to firmware, `<-` is firmware to app.

```
-> CAN_INIT:500000,8
<- diag:can_init,OK,bitrate=500000 clock=8MHz

-> CAN_CONFIG:7E0,7E8,0,AA
<- diag:can_cfg,OK,tx=0x7E0 rx=0x7E8 ...

-> CAN_MONITOR:1
<- diag:can_rx,OK,0x7E8 [8] 06 41 00 BE 3F A8 13 00
<- diag:can_rx,OK,0x123 [3] 01 02 03 | 0x18DAF110 (29-bit) [8] 03 7F 22 31 00 00 00 00 (dropped 2)

-> CAN_TX:7E0,0,02 10 03
<- diag:can_tx,OK,0x7E0 [3] 02 10 03
<- diag:uds,OK,... 50 03 ...

-> CAN_TX:7FF,0,01
<- diag:can_tx,ERR,TX FAILED <reason>        (shown red)

-> DIAG_STOP        (always accepted, even before auth; stops the monitors)
```

`can_rx` frame text format used by the firmware: `0x<ID> [<len>] <HEX bytes>`,
`(29-bit)` after the ID for extended frames, up to 4 frames per line separated
by ` | `, and ` (dropped N)` appended if the ring buffer overflowed. The app
does not parse this text; any readable format works.

### 2.5 Test result example
```
-> RUN_TEST:resistance
<- resistance,PASS,12.4 ohm
<- short_to_ground,FAIL,0.2 ohm to GND
```

## 3. V2 firmware (Bluetooth Classic, `TIFF_TESTER_V2`)

The app polls `STATUS\n` every 500 ms and expects newline-terminated lines. V2
has **no** `diag:` lines and no CAN monitor / UDS / K-Line engine; the
Diagnostics screen is disabled. The only CAN information is the `CAN=` field.

**Status line** (must start with `VOLTAGE=`, otherwise ignored):
```
VOLTAGE=12.40,DUT_VOLTAGE=0.00,CURRENT=0.00,DUT=OFF,FAULT=NO,WATCHDOG=OK,POSITION=0,CAN=READY,KLINE=READY,INA219=READY,R1=OFF,R2=OFF,R3=OFF,R4=OFF,M1=OFF,M2=OFF
```
- `CAN=READY` shows ready; anything else shows an error.
- `FAULT=NO` means no fault; any other value is treated as the fault text.
- Missing `R1` / `M1` means older firmware; the app then shows the last
  command it sent instead of a reading.

**Other lines the app handles:** `FAULT|<text>`, `OK|...`, `ERROR|...`,
anything containing `=BLOCKED`, `PRETEST=PASS|FAIL|...`, `VALIDATION=...`, and
the module list (`MODULE_LIST_BEGIN`, `MODULE|<file>` lines, `MODULE_LIST_END`).

**Commands the app sends:** `POWER_ON`, `POWER_OFF`, `RESET_FAULT`, `ALL_OFF`,
`RELAYn_ON/OFF`, `MOSFETn_ON/OFF`, `SELECT_MODULE|id`, `VALIDATE_MODULE|id`,
`LIST_MODULES`.

V2 refuses `POWER_ON` / `PRETEST` until a module is selected and validated and
the Nano, SD, INA219 and supply are healthy. That is enforced in the firmware,
not the app.

## 4. Summary

| Need | BLE | V2 |
|---|---|---|
| Status stream | JSON on Status char | `VOLTAGE=...` lines |
| Auth | PIN written to Auth char | none |
| CAN ready indicator | `"can":1` | `CAN=READY` |
| CAN live frames | `diag:can_rx,OK,...` | not supported |
| Test results | `type,PASS\|FAIL,msg` | `PRETEST=` / `VALIDATION=` / `OK\|` / `ERROR\|` |

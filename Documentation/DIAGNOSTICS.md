# Diagnostics: CAN, UDS, K-Line, KWP

Added in the diagnostics release. **None of this has been validated against a
real ECU or bus** - bench-test with a CAN analyser / K-Line scope first.

## Architecture

```
App (Diagnostics screen)  --BLE text command-->  ESP32
                                                   | queue (diag_engine.ino)
                                                   v
                         CAN: MCP2515 -> ISO-TP (uds_isotp.ino) -> UDS
                         K-Line: UART -> KWP framing (kline_iso14230.ino)
                                                   |
App console  <--Result characteristic "diag:..."---+--> /LOGS/DIAG.CSV
```

The app never implements a protocol: it sends `UDS_REQUEST:22 F1 90`; the
ESP32 does ISO-TP segmentation, flow control, response-pending handling and
KWP framing/echo stripping, then returns text.

## Safety rules

- **Read-oriented only.** UDS allows `10` (default/extended session only),
  `3E`, `22`, `19`, `14`. KWP allows `10 81`, `3E`, `18`, `17`, `21`, `1A`,
  `14`, `82`. Programming/security-access/routine/IO-control/reset/write
  services and programming sessions are refused in firmware, whatever the
  app sends. There is no flashing support.
- **No relay access.** Diagnostic code cannot switch relays or send the Nano
  anything but the heartbeat. The Nano stays the safety authority.
- **Safety gating.** Every bus command is refused while the Nano reports an
  e-stop or a latched fault.
- **Heartbeat kept alive.** All blocking waits (ISO-TP, K-Line, 5-baud init)
  keep sending the Nano heartbeat, so a long exchange cannot drop the DUT relay.
- **Authenticated.** All commands need the BLE PIN, except `DIAG_STOP`.
- **Raw CAN TX** (`CAN_TX`) puts arbitrary frames on the bus. The app asks for
  confirmation. Use on a bench module only.

## Commands (BLE Command characteristic)

| Command | Meaning |
|---|---|
| `CAN_INIT:<bitrate>,<clock_mhz>` | 125000/250000/500000/1000000, crystal 8 or 16; saved to `/CONFIG.INI` |
| `CAN_CONFIG:<txHex>,<rxHex>,<ext 0\|1>` | diagnostic IDs (11- or 29-bit) |
| `CAN_TX:<idHex>,<ext>,<dataHex>` | one raw frame, max 8 bytes |
| `CAN_MONITOR:<0\|1>` | stream received frames (batched, rate-limited) |
| `UDS_REQUEST:<hex>` | raw request, whitelist enforced |
| `UDS_SESSION:<01\|03>` / `UDS_READ_DID:<hhhh>` / `UDS_READ_DTC` / `UDS_CLEAR_DTC` / `UDS_TESTER_PRESENT` | helpers; DTCs decoded to P/C/B/U codes |
| `KLINE_CONFIG:<9600\|10400>,<targetHex>,<sourceHex>` | baud and addresses |
| `KLINE_INIT` / `KLINE_5BAUD_INIT` | fast init / ISO 9141 5-baud init |
| `KLINE_MONITOR:<0\|1>` | raw RX byte monitor |
| `KWP_REQUEST:<hex>` / `KWP_START_SESSION` / `KWP_READ_DTC` / `KWP_CLEAR_DTC` / `KWP_TESTER_PRESENT` | KWP2000 |
| `DIAG_STOP` | stop monitors, drop queue |

Not supported on the V2 Bluetooth Classic firmware (the app says so).

## CAN clock

The MCP2515 timing tables cover **8 MHz and 16 MHz** crystals. Use the one
printed on your board; the wrong one gives no communication or bus errors.
Default is 8 MHz / 500 kbit/s; change it from the app (CAN INIT) or with
`can_bitrate=` / `can_clock_mhz=` in `/CONFIG.INI`.

## Module profile `[COMMUNICATION]`

```
protocol=CAN            bitrate=500000
can_tx_id=0x7E0         can_rx_id=0x7E8        can_extended=0
kline_baud=10400        kline_target=0x33      kline_source=0xF1
```

Selecting a module loads these into the engine; a CAN module's bitrate is
applied at runtime (not saved as the default). The app's Module database
carries the same fields and pre-fills the Diagnostics screen for the active
module.

## Logs

- `/LOGS/DIAG.CSV` - `TIME_MS,MODULE,KIND,DATA`: requests, responses, init
  results, CAN TX. Monitor streams are shown live but not logged (volume).
- `/LOGS/TEST_<ms>.CSV` - one row per second while the DUT relay is on:
  supply/DUT voltage, current, power, position, temperature, relays, module.

## Known limits

- ISO-TP pads frames with `0xAA`; some ECUs want a different pad byte.
- CAN receive is polled (no interrupt), fine for diagnostics, can drop frames
  on a busy bus in the monitor.
- 5-baud/fast-init timing follows the standards but ECUs vary.
- UDS `19 02` decoding assumes the standard 4-byte DTC record.

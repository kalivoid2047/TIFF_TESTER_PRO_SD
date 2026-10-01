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
| `CAN_CONFIG:<txHex>,<rxHex>,<ext 0\|1>[,<padHex\|NONE>]` | diagnostic IDs (11- or 29-bit) and ISO-TP padding |
| `CAN_TX:<idHex>,<ext>,<dataHex>` | one raw frame, max 8 bytes |
| `CAN_MONITOR:<0\|1>` | stream received frames (batched, rate-limited) |
| `UDS_REQUEST:<hex>` | raw request, whitelist enforced |
| `UDS_SESSION:<01\|03>` / `UDS_READ_DID:<hhhh>` / `UDS_READ_DTC` / `UDS_CLEAR_DTC` / `UDS_TESTER_PRESENT` | helpers; DTCs decoded to P/C/B/U codes |
| `KLINE_CONFIG:<9600\|10400>,<targetHex>,<sourceHex>` | baud and addresses |
| `KLINE_INIT` / `KLINE_5BAUD_INIT` | fast init / ISO 9141 5-baud init |
| `KLINE_MONITOR:<0\|1>` | raw RX byte monitor |
| `KWP_REQUEST:<hex>` / `KWP_START_SESSION` / `KWP_READ_DTC` / `KWP_CLEAR_DTC` / `KWP_TESTER_PRESENT` | KWP2000 |
| `SELFTEST[:KLINE_ECHO]` | CAN loopback + K-Line idle-level check; `KLINE_ECHO` also pulses the K-Line low |
| `DIAG_STOP` | stop monitors, drop queue |

### V2 Bluetooth Classic firmware

The V2 firmware ([`ESP32_Firmware_V2`](../ESP32_Firmware_V2/README.md)) has no
diagnostics engine: it only checks that the MCP2515 answers a reset and opens
the K-Line UART. The app detects a V2 connection and says so instead of
failing per tap:

- **Diagnostics screen:** banner + controls disabled.
- **Relays:** V2 has Nano relays 1-4 (`RELAY1_ON` ... `RELAY4_OFF`), and the
  app maps its relay switches onto them. V2.2.1 reads their state back from
  the Nano; older V2.2 firmware doesn't, in which case the app shows the
  **last command sent** and says so. Relay tests and polarity control exist
  only on the BLE firmware.
- **Live Data:** position, INA219, CAN, K-LINE and system state are shown (V2
  reports them). **Temperature shows n/a** because V2's `TEMP` is a raw ADC
  voltage placeholder, not degrees C.

Supporting CAN/UDS/KWP on V2 would mean porting the diagnostics engine into
that sketch, which is a separate piece of work.

## CAN clock

The MCP2515 timing tables cover **8 MHz and 16 MHz** crystals. Use the one
printed on your board; the wrong one gives no communication or bus errors.
Default is 8 MHz / 500 kbit/s; change it from the app (CAN INIT) or with
`can_bitrate=` / `can_clock_mhz=` in `/CONFIG.INI`.

## Module profile `[COMMUNICATION]`

```
protocol=CAN            bitrate=500000
can_tx_id=0x...         can_rx_id=0x...        can_extended=0
can_pad_byte=0xAA       can_padding=1
kline_baud=10400        kline_target=0x..      kline_source=0x..
```

**Addressing is never assumed.** The shipped example profile leaves the IDs
unset because the real ones for that module are unverified. If a profile (or
an explicit `CAN_CONFIG` / `KLINE_CONFIG`) doesn't set them, the engine falls
back to generic OBD-II values (CAN `0x7E0`/`0x7E8`, K-Line target `0x33`,
tester `0xF1`) and prints a `WARNING ... unverified for this module` line
before the first request on that bus. The INI parser ignores `#`/`;` comment
lines and only matches keys at the start of a line, so commented examples are
never read as values.

**ISO-TP padding.** Default: pad every frame to 8 bytes with `0xAA`. ECUs
differ - some need `00`/`55`/`CC`, some reject padded frames. Set
`can_pad_byte=` (hex) and/or `can_padding=0` (send short frames), pick it in
the app's CAN tab / module form, or pass it as the 4th `CAN_CONFIG` argument
(`AA`, `00`, ... or `NONE`). If requests time out with correct IDs, try the
other options.

Selecting a module loads these into the engine; a CAN module's bitrate is
applied at runtime (not saved as the default). The app's Module database
carries the same fields and pre-fills the Diagnostics screen for the active
module.

## Self-test

- **CAN loopback:** uses the MCP2515's internal loopback mode - a frame is sent
  and received inside the chip. It verifies SPI, the controller and its
  bit-timing setup and puts **nothing on the bus**, so it is safe with an ECU
  attached. It does *not* test the CAN transceiver or the bus wiring.
- **K-Line idle level:** passive - the line idles high, so a low RX pin means the
  transceiver is unpowered/missing or the line is shorted or held.
- **K-Line echo (`SELFTEST:KLINE_ECHO`):** drives TX low for ~300 us and checks RX
  follows (the L9637D echoes TX). It briefly pulls the K-Line low, so it only
  runs on request, never at boot, and not while an ECU is mid-conversation.

The CAN loopback and the passive K-Line check run at boot (results to the
serial console and `/LOGS/system.log`). The Live Data card shows `READY ✓` after
a passed self-test and `SELF-TEST FAILED` if one failed, because the plain
READY flag only means "initialised".

## Live graph, CSV and reports

The app keeps the last ~600 status readings (about 5 min) for the session. Live
Data > VIEW GRAPH draws supply/DUT voltage, current, power, position and
temperature, and copies the data as CSV (same columns as the tester's
`TEST_*.CSV`). The PDF report gains a "Live data" min/average/max table built
from the same readings. Temperature is left out when the firmware does not
report real degrees C (V2).

## Logs

- `/LOGS/DIAG.CSV` - `TIME_MS,MODULE,KIND,DATA`: requests, responses, init
  results, CAN TX. Monitor streams are shown live but not logged (volume).
- `/LOGS/TEST_<ms>.CSV` - one row per second while the DUT relay is on:
  supply/DUT voltage, current, power, position, temperature, relays, module.

## Known limits

- CAN receive is polled (no interrupt), fine for diagnostics, can drop frames
  on a busy bus in the monitor.
- 5-baud/fast-init timing follows the standards but ECUs vary.
- UDS `19 02` decoding assumes the standard 4-byte DTC record.

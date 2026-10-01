// K-Line driver (ISO 9141-2 / ISO 14230 KWP2000) over a spare ESP32 UART.
//
// The L9637D on the wiring doc is just a K-Line transceiver (line driver) -
// the actual protocol lives in software, which is what this file provides:
//   - ISO 14230 fast init (25 ms low / 25 ms high wake-up + StartCommunication)
//   - ISO 9141 / 14230 5-baud slow init (address sent at 5 baud, sync byte,
//     key bytes, inverted-key-byte-2 / inverted-address handshake)
//   - KWP framing with configurable target/source addresses, checksum,
//     echo stripping (the L9637D echoes TX onto RX) and NRC 0x78 handling
//   - a restricted KWP client (kwpAllowed()): read-oriented services only
//
// SAFETY / SCOPE: programming, security access, routine control, ECU reset,
// write-by-identifier etc. are refused regardless of what the app asks for
// (see Documentation/DIAGNOSTICS.md). Nothing here touches the relays.
//
// IMPORTANT: none of this has been validated against a real K-Line ECU.
// Slow-init timing and ECU behaviour vary by manufacturer - bench-test with
// a scope/logic analyser before trusting it against a vehicle module.

#define KLINE_BAUD 10400 // default; 9600 also supported via klineSetBaud()

long klineBaud = KLINE_BAUD;
String klineError = "";

// True once the UART is open. This does NOT mean an ECU answered - the
// L9637D transceiver can't be probed from here; klineFastInit()/
// klineSlowInit() are what tell you whether an ECU responds.
bool klineReady = false;
int8_t klineSelfTestResult = -1; // -1 not run, 0 failed, 1 passed (most recent check)

void klineInit() {
  KlineSerial.begin(klineBaud, SERIAL_8N1, KLINE_RX, KLINE_TX);
  klineReady = true;
}

// 9600 and 10400 only; restarts the UART at the new rate.
bool klineSetBaud(long baud) {
  if (baud != 9600 && baud != 10400) return false;
  klineBaud = baud;
  KlineSerial.end();
  KlineSerial.begin(klineBaud, SERIAL_8N1, KLINE_RX, KLINE_TX);
  klineReady = true;
  return true;
}

// delay() that keeps the Nano heartbeat flowing: slow init takes >2 s, longer
// than the Nano's 2 s dead-man timeout, which would otherwise drop the DUT relay.
static void klineDelay(uint32_t ms) {
  uint32_t end = millis() + ms;
  while ((int32_t)(end - millis()) > 0) serviceHeartbeat();
}

static void klineFlushRx() {
  while (KlineSerial.available()) KlineSerial.read();
}

// Reads one byte within timeoutMs; keeps the Nano heartbeat alive.
static bool klineReadByte(uint8_t &b, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (KlineSerial.available()) { b = (uint8_t)KlineSerial.read(); return true; }
    serviceHeartbeat();
  }
  return false;
}

// Collects a burst: waits up to firstTimeoutMs for the first byte, then
// reads until the line is idle for gapMs. Returns the byte count.
static uint8_t klineReadBurst(uint8_t *buf, uint8_t maxLen, uint32_t firstTimeoutMs, uint32_t gapMs) {
  uint8_t n = 0, b;
  if (!klineReadByte(b, firstTimeoutMs)) return 0;
  buf[n++] = b;
  while (n < maxLen && klineReadByte(b, gapMs)) buf[n++] = b;
  return n;
}

// Builds a KWP frame: [fmt][target][source][data...][checksum]. fmt =
// `fmtBase`|len (0x80 physical / 0xC0 functional addressing, address bytes
// present) for len <= 63.
static uint8_t klineBuildFrame(uint8_t fmtBase, uint8_t target, uint8_t source,
                               const uint8_t *data, uint8_t len, uint8_t *frame) {
  uint8_t idx = 0;
  frame[idx++] = fmtBase | len;
  frame[idx++] = target;
  frame[idx++] = source;
  for (uint8_t i = 0; i < len; i++) frame[idx++] = data[i];
  uint8_t cs = 0;
  for (uint8_t i = 0; i < idx; i++) cs += frame[i];
  frame[idx++] = cs;
  return idx;
}

static void klineWriteFrame(const uint8_t *frame, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    KlineSerial.write(frame[i]);
    delay(2); // P4: inter-byte time from the tester
  }
  KlineSerial.flush();
}

// If the burst starts with an echo of what we sent, drop it. Returns the
// offset of the first non-echo byte.
static uint8_t klineEchoSkip(const uint8_t *rx, uint8_t rxLen, const uint8_t *tx, uint8_t txLen) {
  if (rxLen >= txLen && memcmp(rx, tx, txLen) == 0) return txLen;
  return 0;
}

// Parses one KWP frame at buf (n bytes). Returns payload length, or -1.
static int klineParseFrame(const uint8_t *buf, uint8_t n, uint8_t *payload, uint8_t maxPayload) {
  if (n < 2) return -1;
  uint8_t fmt = buf[0];
  uint8_t len = fmt & 0x3F;
  uint8_t idx = 1;
  if (fmt & 0xC0) idx += 2;            // target + source present
  if (len == 0) { if (idx >= n) return -1; len = buf[idx++]; }
  if (len == 0 || len > maxPayload || (uint16_t)idx + len + 1 > n) return -1;
  uint8_t cs = 0;
  for (uint8_t i = 0; i < idx + len; i++) cs += buf[i];
  if (cs != buf[idx + len]) return -1;
  memcpy(payload, buf + idx, len);
  return len;
}

// ISO 14230 fast init with explicit addresses. Returns true only if the ECU
// answered with a valid StartCommunication response beyond our own echo.
bool klineFastInitAddr(uint8_t target, uint8_t source) {
  klineError = "";
  KlineSerial.end();
  pinMode(KLINE_TX, OUTPUT);
  digitalWrite(KLINE_TX, HIGH);
  klineDelay(300);             // bus idle before the wake-up pattern
  digitalWrite(KLINE_TX, LOW);
  klineDelay(25);
  digitalWrite(KLINE_TX, HIGH);
  klineDelay(25);

  KlineSerial.begin(klineBaud, SERIAL_8N1, KLINE_RX, KLINE_TX);
  klineFlushRx();

  uint8_t req[1] = {0x81};     // StartCommunication
  uint8_t frame[8];
  uint8_t fl = klineBuildFrame(0xC0, target, source, req, 1, frame); // 0xC1 ...
  klineWriteFrame(frame, fl);

  uint8_t rx[24];
  uint8_t n = klineReadBurst(rx, sizeof(rx), 300, 30);
  uint8_t skip = klineEchoSkip(rx, n, frame, fl);
  if (n <= skip) { klineError = "NO_RESPONSE"; return false; }

  uint8_t payload[16];
  int pl = klineParseFrame(rx + skip, n - skip, payload, sizeof(payload));
  if (pl < 1 || payload[0] != 0xC1) { klineError = "BAD_STARTCOMM_RESPONSE " + hexOf(rx + skip, n - skip); return false; }
  return true;
}

// Original entry point (functional address 0x33, tester 0xF1); kept for
// existing callers. Now ignores the echo of its own request, so it no longer
// reports success when no ECU is attached.
bool klineFastInit() {
  return klineFastInitAddr(0x33, 0xF1);
}

// ISO 9141 / 14230 5-baud initialisation. `addr` is the ECU address sent at
// 5 baud. Returns true if the ECU completed the handshake; kb1/kb2 are the
// key bytes it reported.
bool klineSlowInit(uint8_t addr, uint8_t &kb1, uint8_t &kb2) {
  klineError = "";
  KlineSerial.end();
  pinMode(KLINE_TX, OUTPUT);
  digitalWrite(KLINE_TX, HIGH);
  klineDelay(300);                     // W0: bus idle

  // 5 baud = 200 ms per bit: start bit (low), 8 data bits LSB first, stop bit (high).
  digitalWrite(KLINE_TX, LOW);
  klineDelay(200);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(KLINE_TX, ((addr >> i) & 1) ? HIGH : LOW);
    klineDelay(200);
  }
  digitalWrite(KLINE_TX, HIGH);
  klineDelay(200);

  KlineSerial.begin(klineBaud, SERIAL_8N1, KLINE_RX, KLINE_TX);
  klineFlushRx();

  uint8_t b;
  if (!klineReadByte(b, 400) || b != 0x55) { klineError = "NO_SYNC_BYTE"; return false; }
  if (!klineReadByte(kb1, 100) || !klineReadByte(kb2, 100)) { klineError = "NO_KEY_BYTES"; return false; }

  klineDelay(30);                      // W4: 25-50 ms before the tester answers
  uint8_t inv = (uint8_t)~kb2;
  KlineSerial.write(inv);
  KlineSerial.flush();

  // Expect the inverted address back; the first byte may be our own echo.
  uint8_t want = (uint8_t)~addr;
  for (int i = 0; i < 3; i++) {
    if (!klineReadByte(b, 150)) break;
    if (b == want) return true;
  }
  klineError = "NO_ADDRESS_ECHO";
  return false;
}

// Read-oriented KWP2000 services only; everything else is refused.
bool kwpAllowed(const uint8_t *req, uint8_t len, String &why) {
  if (len < 1) { why = "EMPTY_REQUEST"; return false; }
  switch (req[0]) {
    case 0x10: // StartDiagnosticSession: default session only (0x85 = programming)
      if (len == 2 && req[1] == 0x81) return true;
      why = "BLOCKED_SESSION (only 0x81 default session allowed)";
      return false;
    case 0x3E: // TesterPresent
      if (len == 2 && (req[1] == 0x01 || req[1] == 0x02)) return true;
      break;
    case 0x18: // ReadDTCByStatus
    case 0x17: // ReadStatusOfDTC
    case 0x21: // ReadDataByLocalIdentifier
    case 0x1A: // ReadECUIdentification
      if (len >= 2) return true;
      break;
    case 0x14: // ClearDiagnosticInformation
      if (len == 3) return true;
      break;
    case 0x82: // StopCommunication
      if (len == 1) return true;
      break;
    default:
      why = "BLOCKED_SERVICE 0x" + String(req[0], HEX) + " (this tester only allows 10/3E/18/17/21/1A/14/82)";
      return false;
  }
  why = "BAD_REQUEST_FORMAT";
  return false;
}

// Sends one KWP request and returns the response payload length (>0), or -1
// with `err` set. Handles echo stripping and NRC 0x78 (response pending).
int kwpExchange(uint8_t target, uint8_t source, const uint8_t *req, uint8_t reqLen,
                uint8_t *resp, uint8_t maxResp, String &err) {
  if (!kwpAllowed(req, reqLen, err)) return -1;
  if (reqLen > 63) { err = "REQUEST_TOO_LONG"; return -1; }

  klineFlushRx();
  uint8_t frame[72];
  uint8_t fl = klineBuildFrame(0x80, target, source, req, reqLen, frame);
  klineWriteFrame(frame, fl);

  uint8_t rx[96];
  uint32_t wait = 300; // P2max is ~50 ms nominally; be tolerant on a bench
  for (int pending = 0; pending < 20; pending++) {
    uint8_t n = klineReadBurst(rx, sizeof(rx), wait, 30);
    uint8_t skip = 0;
    if (pending == 0) skip = klineEchoSkip(rx, n, frame, fl);
    if (n <= skip) { err = "NO_RESPONSE"; return -1; }
    int pl = klineParseFrame(rx + skip, n - skip, resp, maxResp);
    if (pl < 0) { err = "BAD_FRAME " + hexOf(rx + skip, n - skip); return -1; }
    if (pl >= 3 && resp[0] == 0x7F && resp[2] == 0x78) { wait = 5000; continue; }
    return pl;
  }
  err = "TOO_MANY_PENDING_RESPONSES";
  return -1;
}

// ---- Original raw helpers, kept for existing callers ----

bool klineSendRequest(const uint8_t *data, uint8_t len) {
  if (len > 6) return false; // keep the simplified fixed-size frame below in bounds

  uint8_t frame[10];
  uint8_t idx = 0;
  frame[idx++] = 0x80 | len; // format byte: physical addressing, length in low bits
  frame[idx++] = 0x33;       // target (tester-to-ECU functional address, simplified)
  frame[idx++] = 0xF1;       // source (tester)
  for (uint8_t i = 0; i < len; i++) frame[idx++] = data[i];

  uint8_t checksum = 0;
  for (uint8_t i = 0; i < idx; i++) checksum += frame[i];
  frame[idx++] = checksum;

  for (uint8_t i = 0; i < idx; i++) KlineSerial.write(frame[i]);
  KlineSerial.flush();
  return true;
}

bool klineReadResponse(uint8_t *buf, uint8_t &len, uint32_t timeoutMs) {
  len = 0;
  uint32_t start = millis();
  while (millis() - start < timeoutMs && len < 32) {
    if (KlineSerial.available()) {
      buf[len++] = KlineSerial.read();
    }
  }
  return len > 0;
}

// Passive K-Line check: the line idles HIGH (pulled to battery by the bus), so a
// LOW RX pin means the transceiver is unpowered/missing or the line is shorted
// or held. Drives nothing, so it is safe with an ECU connected.
bool klineIdleCheck(String &detail) {
  bool high = digitalRead(KLINE_RX) == HIGH;
  detail = high ? "RX idle high" : "RX held low at idle (transceiver unpowered, K-Line shorted or held by a device)";
  klineSelfTestResult = high ? 1 : 0;
  return high;
}

// Active K-Line echo check: drives TX low for ~300 us and confirms RX follows
// (the L9637D echoes TX onto RX). This briefly pulls the K-Line low, so only run
// it on request and not while a vehicle/ECU is mid-conversation.
bool klineEchoCheck(String &detail) {
  KlineSerial.end();
  pinMode(KLINE_TX, OUTPUT);
  pinMode(KLINE_RX, INPUT);
  digitalWrite(KLINE_TX, HIGH);
  delay(2);
  bool idleHigh = digitalRead(KLINE_RX) == HIGH;
  digitalWrite(KLINE_TX, LOW);
  delayMicroseconds(300);
  bool followsLow = digitalRead(KLINE_RX) == LOW;
  digitalWrite(KLINE_TX, HIGH);
  delay(2);
  bool highAgain = digitalRead(KLINE_RX) == HIGH;
  KlineSerial.begin(klineBaud, SERIAL_8N1, KLINE_RX, KLINE_TX);

  bool ok = idleHigh && followsLow && highAgain;
  if (ok) detail = "TX echoes on RX";
  else if (!idleHigh) detail = "RX not high with TX high";
  else if (!followsLow) detail = "RX did not follow TX low (transceiver unpowered or not wired)";
  else detail = "RX did not return high";
  klineSelfTestResult = ok ? 1 : 0;
  return ok;
}

// ISO-TP (ISO 15765-2) transport and a restricted UDS (ISO 14229) client on
// top of the MCP2515 driver (can_mcp2515.ino).
//
// SAFETY / SCOPE: only read-oriented diagnostic services are allowed
// (udsAllowed() below). Programming, security access, routine control, IO
// control, ECU reset, write-by-identifier and the programming/safety
// sessions are refused no matter what the app asks for - this tester does
// not flash or reconfigure ECUs. Nothing here touches the relays; the Nano
// remains the sole safety authority (see Documentation/DIAGNOSTICS.md).
//
// All waits call serviceHeartbeat() so the Nano's dead-man heartbeat keeps
// flowing while a (blocking) exchange is in progress.
//
// NOT validated against a real ECU. Bench-test with a CAN analyser first.

#define ISOTP_TIMEOUT_MS   1000   // N_Bs / N_Cr
#define UDS_P2_MS          1000   // normal response wait
#define UDS_P2_STAR_MS     5000   // after NRC 0x78 (response pending)

String isotpError = "";

// Frame padding. Default is pad-to-8 with 0xAA, but ECUs differ: some require
// a specific pad byte (00/55/CC), some reject padded frames. Set per module
// (can_pad_byte= / can_padding= in [COMMUNICATION]) or via CAN_CONFIG.
uint8_t isotpPadByte = 0xAA;
bool isotpPadding = true;

static void isotpStMin(uint8_t st) {
  if (st <= 0x7F) { if (st) delay(st); }
  else if (st >= 0xF1 && st <= 0xF9) delayMicroseconds((st - 0xF0) * 100);
  else if (st) delay(127); // reserved values: be conservative
}

static bool isotpSendFrame(uint32_t id, bool ext, const uint8_t *payload, uint8_t n) {
  uint8_t f[8];
  memset(f, isotpPadByte, sizeof(f));
  memcpy(f, payload, n);
  return canSendMessageExt(id, ext, f, isotpPadding ? 8 : n);
}

// Waits for a frame with exactly (rxId, ext). Other traffic is dropped.
static bool isotpWaitFrame(uint32_t rxId, bool ext, uint8_t *f, uint8_t &len, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    serviceHeartbeat();
    uint32_t id;
    bool e;
    uint8_t data[8], l;
    if (canReceiveFrame(id, e, data, l)) {
      if (id == rxId && e == ext) {
        memcpy(f, data, l);
        len = l;
        return true;
      }
    }
  }
  return false;
}

// Returns true on success; on failure isotpError says why.
bool isotpSend(uint32_t txId, uint32_t rxId, bool ext, const uint8_t *data, uint16_t len) {
  isotpError = "";
  if (len == 0 || len > 4095) { isotpError = "BAD_LENGTH"; return false; }

  uint8_t f[8];
  if (len <= 7) {
    f[0] = (uint8_t)len; // single frame
    memcpy(f + 1, data, len);
    if (!isotpSendFrame(txId, ext, f, len + 1)) { isotpError = "CAN_TX_FAILED " + canErrorText(); return false; }
    return true;
  }

  // First frame: 12-bit length + 6 data bytes.
  f[0] = 0x10 | (uint8_t)(len >> 8);
  f[1] = (uint8_t)(len & 0xFF);
  memcpy(f + 2, data, 6);
  if (!isotpSendFrame(txId, ext, f, 8)) { isotpError = "CAN_TX_FAILED " + canErrorText(); return false; }

  uint16_t sent = 6;
  uint8_t seq = 1, bs = 0, st = 0, blockCount = 0;
  bool needFc = true;

  while (sent < len) {
    if (needFc) {
      uint8_t fc[8], fl;
      uint8_t waits = 0;
      for (;;) {
        if (!isotpWaitFrame(rxId, ext, fc, fl, ISOTP_TIMEOUT_MS)) { isotpError = "NO_FLOW_CONTROL"; return false; }
        if ((fc[0] >> 4) != 3) continue;           // not a flow-control frame
        uint8_t fs = fc[0] & 0x0F;
        if (fs == 0) { bs = fc[1]; st = fc[2]; break; }   // continue to send
        if (fs == 1) { if (++waits > 10) { isotpError = "FC_WAIT_TIMEOUT"; return false; } continue; }
        isotpError = "FC_OVERFLOW"; return false;
      }
      needFc = false;
      blockCount = 0;
    }

    uint8_t n = (len - sent) > 7 ? 7 : (uint8_t)(len - sent);
    f[0] = 0x20 | (seq & 0x0F);
    memcpy(f + 1, data + sent, n);
    if (!isotpSendFrame(txId, ext, f, n + 1)) { isotpError = "CAN_TX_FAILED " + canErrorText(); return false; }
    seq++;
    sent += n;
    if (sent < len) {
      isotpStMin(st);
      if (bs && ++blockCount == bs) needFc = true;
    }
  }
  return true;
}

// Receives one ISO-TP message. Returns its length, or -1 on error/timeout
// (isotpError set).
int isotpReceive(uint32_t rxId, uint32_t txId, bool ext, uint8_t *buf, uint16_t maxLen, uint32_t timeoutMs) {
  isotpError = "";
  uint32_t start = millis();
  uint8_t f[8], fl;

  // Wait for a SF/FF (ignore stray flow-control/consecutive frames).
  for (;;) {
    uint32_t left = timeoutMs > (millis() - start) ? timeoutMs - (millis() - start) : 0;
    if (!left || !isotpWaitFrame(rxId, ext, f, fl, left)) { isotpError = "TIMEOUT"; return -1; }
    uint8_t type = f[0] >> 4;
    if (type == 0) {
      uint8_t l = f[0] & 0x0F;
      if (l == 0 || l > 7 || l > maxLen || l > fl - 1) { isotpError = "BAD_SINGLE_FRAME"; return -1; }
      memcpy(buf, f + 1, l);
      return l;
    }
    if (type == 1) break;
  }

  uint16_t total = ((uint16_t)(f[0] & 0x0F) << 8) | f[1];
  if (total < 8 || total > maxLen) {
    uint8_t fc[3] = {0x32, 0, 0}; // overflow
    isotpSendFrame(txId, ext, fc, 3);
    isotpError = "RESPONSE_TOO_LONG";
    return -1;
  }
  memcpy(buf, f + 2, 6);
  uint16_t got = 6;

  uint8_t fc[3] = {0x30, 0, 0}; // clear to send, no block limit, no separation
  if (!isotpSendFrame(txId, ext, fc, 3)) { isotpError = "CAN_TX_FAILED " + canErrorText(); return -1; }

  uint8_t expect = 1;
  while (got < total) {
    if (!isotpWaitFrame(rxId, ext, f, fl, ISOTP_TIMEOUT_MS)) { isotpError = "CF_TIMEOUT"; return -1; }
    if ((f[0] >> 4) != 2) continue;
    if ((f[0] & 0x0F) != expect) { isotpError = "BAD_SEQUENCE"; return -1; }
    uint8_t n = (total - got) > 7 ? 7 : (uint8_t)(total - got);
    memcpy(buf + got, f + 1, n);
    got += n;
    expect = (expect + 1) & 0x0F;
  }
  return total;
}

// ---------- hex helpers ----------

String hexOf(const uint8_t *d, uint16_t n) {
  String s;
  for (uint16_t i = 0; i < n; i++) {
    if (i) s += " ";
    if (d[i] < 0x10) s += "0";
    s += String(d[i], HEX);
  }
  s.toUpperCase();
  return s;
}

// Parses "22 F1 90", "22,F1,90", "0x22 0xF1 0x90" or "22F190" into bytes.
// Returns the byte count, or -1 on bad input / overflow.
int hexParse(const String &in, uint8_t *out, int maxLen) {
  int n = 0;
  String tok;
  String s = in + " ";
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == ' ' || c == ',' || c == '\t') {
      if (tok.length()) {
        if (tok.startsWith("0x") || tok.startsWith("0X")) tok = tok.substring(2);
        if (tok.length() == 0 || (tok.length() % 2) != 0) return -1;
        for (unsigned k = 0; k < tok.length(); k += 2) {
          if (n >= maxLen) return -1;
          char h[3] = {tok[k], tok[k + 1], 0};
          if (!isxdigit(h[0]) || !isxdigit(h[1])) return -1;
          out[n++] = (uint8_t)strtoul(h, nullptr, 16);
        }
        tok = "";
      }
    } else {
      tok += c;
    }
  }
  return n;
}

// ---------- UDS ----------

// Whitelist of read-oriented services. Everything else is refused.
bool udsAllowed(const uint8_t *req, uint16_t len, String &why) {
  if (len < 1) { why = "EMPTY_REQUEST"; return false; }
  switch (req[0]) {
    case 0x10: // DiagnosticSessionControl: default (01) / extended (03) only
      if (len == 2 && ((req[1] & 0x7F) == 0x01 || (req[1] & 0x7F) == 0x03)) return true;
      why = "BLOCKED_SESSION (only default/extended sessions allowed)";
      return false;
    case 0x3E: // TesterPresent
      if (len == 2 && (req[1] & 0x7F) == 0x00) return true;
      break;
    case 0x22: // ReadDataByIdentifier
      if (len >= 3 && ((len - 1) % 2) == 0) return true;
      break;
    case 0x19: // ReadDTCInformation
      if (len >= 2) return true;
      break;
    case 0x14: // ClearDiagnosticInformation (group of DTC)
      if (len == 4) return true;
      break;
    default:
      why = "BLOCKED_SERVICE 0x" + String(req[0], HEX) + " (this tester only allows 10/3E/22/19/14)";
      return false;
  }
  why = "BAD_REQUEST_FORMAT";
  return false;
}

// Sends one UDS request on the given IDs and returns the response length
// (>0), or -1 on failure with `err` set. Handles NRC 0x78 (response
// pending). Negative responses are returned as data (resp[0]==0x7F).
int udsExchange(uint32_t txId, uint32_t rxId, bool ext, const uint8_t *req, uint16_t reqLen,
                uint8_t *resp, uint16_t maxResp, String &err) {
  if (!udsAllowed(req, reqLen, err)) return -1;
  if (!canReady) { err = "CAN_NOT_READY"; return -1; }

  // Drop anything stale so we don't mistake an old frame for the answer.
  uint32_t id; bool e; uint8_t d[8], l;
  for (int i = 0; i < 16 && canReceiveFrame(id, e, d, l); i++) {}

  if (!isotpSend(txId, rxId, ext, req, reqLen)) { err = isotpError; return -1; }

  uint32_t wait = UDS_P2_MS;
  for (int pending = 0; pending < 20; pending++) {
    int n = isotpReceive(rxId, txId, ext, resp, maxResp, wait);
    if (n < 0) { err = isotpError; return -1; }
    if (n >= 3 && resp[0] == 0x7F && resp[2] == 0x78) { wait = UDS_P2_STAR_MS; continue; }
    return n;
  }
  err = "TOO_MANY_PENDING_RESPONSES";
  return -1;
}

// "P0301-1A"-style text for a 3-byte DTC (two code bytes + failure-type byte).
String dtcText(uint8_t b1, uint8_t b2, uint8_t b3, bool withFailureType) {
  static const char letters[4] = {'P', 'C', 'B', 'U'};
  String s;
  s += letters[(b1 >> 6) & 3];
  s += String((b1 >> 4) & 3);
  s += String(b1 & 0x0F, HEX);
  s += String(b2 >> 4, HEX);
  s += String(b2 & 0x0F, HEX);
  if (withFailureType) { s += "-"; s += String(b3 >> 4, HEX); s += String(b3 & 0x0F, HEX); }
  s.toUpperCase();
  return s;
}

String udsNrcText(uint8_t nrc) {
  switch (nrc) {
    case 0x10: return "generalReject";
    case 0x11: return "serviceNotSupported";
    case 0x12: return "subFunctionNotSupported";
    case 0x13: return "incorrectMessageLengthOrFormat";
    case 0x22: return "conditionsNotCorrect";
    case 0x31: return "requestOutOfRange";
    case 0x33: return "securityAccessDenied";
    case 0x78: return "responsePending";
    case 0x7E: return "subFunctionNotSupportedInActiveSession";
    case 0x7F: return "serviceNotSupportedInActiveSession";
    default:   return "NRC";
  }
}

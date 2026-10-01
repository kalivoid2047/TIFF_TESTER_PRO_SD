// Diagnostics engine: CAN / UDS / K-Line / KWP commands from the app.
//
// Flow: the BLE command handler calls diagHandleCommand(), which only
// validates and ENQUEUES. diagLoop() (called from loop()) executes one queued
// command at a time, so blocking bus exchanges never run inside the BLE
// callback. Output goes to the app as Result-characteristic notifications
// shaped "diag:<kind>,<OK|ERR>,<text>" (the app routes the "diag:" prefix to
// its Diagnostics console instead of the Results list) and is appended to
// /LOGS/DIAG.CSV.
//
// Commands (see Documentation/DIAGNOSTICS.md for the full table):
//   CAN_INIT:<bitrate>,<clock_mhz>      CAN_CONFIG:<txHex>,<rxHex>,<ext 0|1>[,<padHex|NONE>]
//   CAN_TX:<idHex>,<ext 0|1>,<dataHex>  CAN_MONITOR:<0|1>
//   UDS_REQUEST:<hex>  UDS_SESSION:<hex>  UDS_READ_DID:<hex>  UDS_READ_DTC
//   UDS_CLEAR_DTC  UDS_TESTER_PRESENT
//   KLINE_CONFIG:<baud>,<targetHex>,<sourceHex>  KLINE_INIT  KLINE_5BAUD_INIT
//   KLINE_MONITOR:<0|1>
//   KWP_REQUEST:<hex>  KWP_START_SESSION  KWP_READ_DTC  KWP_CLEAR_DTC
//   KWP_TESTER_PRESENT
//   DIAG_STOP                           (always allowed: stops monitors, drops queue)
//
// SAFETY: nothing in this tab can switch a relay or send a command to the
// Nano other than the heartbeat (serviceHeartbeat). Every bus command is
// refused while the Nano reports an e-stop or latched fault, and the UDS/KWP
// layers only allow the read-oriented service whitelists (uds_isotp.ino,
// kline_iso14230.ino).

// Arduino concatenates tabs alphabetically, so variables defined in tabs that
// sort after this one (kline_iso14230, uds_isotp) must be declared here.
extern String klineError;
extern uint8_t isotpPadByte;
extern bool isotpPadding;

#define DIAG_QUEUE_LEN   6
#define DIAG_LINE_MAX    170     // keep each notification inside the BLE MTU
#define DIAG_LOG_PATH    "/LOGS/DIAG.CSV"
#define CAN_RING_LEN     32

static String diagQueue[DIAG_QUEUE_LEN];
static volatile uint8_t diagHead = 0, diagTail = 0;

// Addressing for UDS/KWP; overwritten by the active module's [COMMUNICATION]
// section (diagApplyModule) or by CAN_CONFIG / KLINE_CONFIG.
static uint32_t diagCanTx = 0x7E0, diagCanRx = 0x7E8;
static bool diagCanExt = false;
static uint8_t diagKTarget = 0x33, diagKSource = 0xF1;

// False while the generic default addressing is in use (nothing from a
// module profile or an explicit CAN_CONFIG/KLINE_CONFIG). The first bus
// command then prints a warning, so default IDs are never mistaken for
// verified ones.
static bool diagCanAddrExplicit = false;
static bool diagKAddrExplicit = false;
static bool diagCanWarned = false;
static bool diagKWarned = false;

static volatile bool canMonitorOn = false;
static volatile bool klineMonitorOn = false;
static CanFrameRec canRing[CAN_RING_LEN];
static uint8_t canRingHead = 0, canRingTail = 0;
static uint16_t canRingDropped = 0;
static uint32_t lastCanEmit = 0;
static uint8_t klineMonBuf[24];
static uint8_t klineMonLen = 0;
static uint32_t klineMonLastByte = 0;

// ---------- output ----------

static void diagLog(const String &kind, const String &text) {
  if (!SD.exists(DIAG_LOG_PATH)) {
    File h = SD.open(DIAG_LOG_PATH, FILE_WRITE);
    if (h) { h.println("TIME_MS,MODULE,KIND,DATA"); h.close(); }
  }
  File f = SD.open(DIAG_LOG_PATH, FILE_APPEND);
  if (!f) return;
  String t = text;
  t.replace("\"", "'");
  f.print(millis());
  f.print(",");
  f.print(activeModule.loaded ? activeModule.id : String("UNKNOWN"));
  f.print(",");
  f.print(kind);
  f.print(",\"");
  f.print(t);
  f.println("\"");
  f.close();
}

// kind: short lowercase tag; ok=false marks an error line in the app console.
static void diagEmit(const String &kind, bool ok, const String &text, bool log = true) {
  String line = "diag:" + kind + "," + (ok ? "OK" : "ERR") + "," + text;
  if (line.length() > DIAG_LINE_MAX) line = line.substring(0, DIAG_LINE_MAX);
  Serial.println(line);
  if (bleClientConnected && bleResultChar) {
    bleResultChar->setValue(line.c_str());
    bleResultChar->notify();
  }
  if (log) diagLog(kind, text);
}

// ---------- queue ----------

static bool diagPush(const String &cmd) {
  uint8_t next = (diagHead + 1) % DIAG_QUEUE_LEN;
  if (next == diagTail) return false; // full
  diagQueue[diagHead] = cmd;
  diagHead = next;
  return true;
}

static bool diagPop(String &cmd) {
  if (diagTail == diagHead) return false;
  cmd = diagQueue[diagTail];
  diagTail = (diagTail + 1) % DIAG_QUEUE_LEN;
  return true;
}

// ---------- module addressing ----------

// Called by loadActiveModule() after a module is selected.
void diagApplyModule() {
  if (!activeModule.loaded) return;
  diagCanTx = activeModule.canTxId;
  diagCanRx = activeModule.canRxId;
  diagCanExt = activeModule.canExtended;
  diagKTarget = activeModule.klineTarget;
  diagKSource = activeModule.klineSource;
  isotpPadByte = activeModule.canPadByte;
  isotpPadding = activeModule.canPadding;
  diagCanAddrExplicit = activeModule.canIdsDefined;
  diagKAddrExplicit = activeModule.klineAddrDefined;
  diagCanWarned = diagKWarned = false;
  if (activeModule.klineBaud == 9600 || activeModule.klineBaud == 10400) klineSetBaud(activeModule.klineBaud);

  // A CAN module's bitrate is applied without persisting it as the default.
  if (activeModule.commProtocol.equalsIgnoreCase("CAN")) {
    long br = activeModule.commBitrate;
    if (br == 125000 || br == 250000 || br == 500000 || br == 1000000) diagPush("CAN_APPLY:" + String(br));
  }
}

// ---------- CAN monitor ring ----------

static void canRingPush(uint32_t id, bool ext, const uint8_t *d, uint8_t len) {
  uint8_t next = (canRingHead + 1) % CAN_RING_LEN;
  if (next == canRingTail) { canRingDropped++; return; }
  canRing[canRingHead].id = id;
  canRing[canRingHead].ext = ext;
  canRing[canRingHead].len = len;
  memcpy(canRing[canRingHead].data, d, len);
  canRingHead = next;
}

// Runs from loop(): drain the controller into the ring, then emit batches
// (up to 4 frames per notification, at most one notification per 60 ms).
static void canMonitorLoop() {
  if (!canMonitorOn) return;

  uint32_t id; bool ext; uint8_t d[8], l;
  for (int i = 0; i < 8 && canReceiveFrame(id, ext, d, l); i++) canRingPush(id, ext, d, l);

  if (canRingTail == canRingHead && !canRingDropped) return;
  if (millis() - lastCanEmit < 60) return;
  lastCanEmit = millis();

  String line;
  for (int n = 0; n < 4 && canRingTail != canRingHead; n++) {
    CanFrameRec &r = canRing[canRingTail];
    if (n) line += " | ";
    String idHex = String(r.id, HEX);
    idHex.toUpperCase();
    line += "0x" + idHex + (r.ext ? " (29-bit)" : "") + " [" + String(r.len) + "] " + hexOf(r.data, r.len);
    canRingTail = (canRingTail + 1) % CAN_RING_LEN;
  }
  if (canRingDropped) {
    line += " (dropped " + String(canRingDropped) + ")";
    canRingDropped = 0;
  }
  diagEmit("can_rx", true, line, false); // monitor traffic is not written to SD (too voluminous)
}

static void klineMonitorLoop() {
  if (!klineMonitorOn) return;
  while (KlineSerial.available() && klineMonLen < sizeof(klineMonBuf)) {
    klineMonBuf[klineMonLen++] = (uint8_t)KlineSerial.read();
    klineMonLastByte = millis();
  }
  if (klineMonLen && (klineMonLen >= sizeof(klineMonBuf) || millis() - klineMonLastByte > 20)) {
    diagEmit("kline_rx", true, hexOf(klineMonBuf, klineMonLen), false);
    klineMonLen = 0;
  }
}

// ---------- command execution ----------

static void emitUdsResult(const uint8_t *resp, int n, const String &what) {
  if (resp[0] == 0x7F && n >= 3) {
    diagEmit("uds", false, what + " NEGATIVE 0x" + String(resp[2], HEX) + " (" + udsNrcText(resp[2]) + ") " + hexOf(resp, n));
  } else {
    diagEmit("uds", true, what + " " + hexOf(resp, n));
  }
}

static void runUds(const uint8_t *req, uint16_t len, const String &what, uint8_t kind) {
  // kind: 0 plain, 1 DTC list decode, 2 clear
  if (!diagCanAddrExplicit && !diagCanWarned) {
    diagCanWarned = true;
    diagEmit("diag", false, "WARNING: no CAN IDs set by the module profile or CAN_CONFIG - using generic 0x" +
                                String(diagCanTx, HEX) + "/0x" + String(diagCanRx, HEX) + ", unverified for this module");
  }
  uint8_t resp[256];
  String err;
  diagLog("uds_req", what + " " + hexOf(req, len));
  int n = udsExchange(diagCanTx, diagCanRx, diagCanExt, req, len, resp, sizeof(resp), err);
  if (n < 0) { diagEmit("uds", false, what + " FAILED: " + err); return; }

  if (kind == 1 && resp[0] == 0x59 && n >= 3) {
    // 59 02 <availabilityMask> then 4-byte records: DTC hi, DTC lo, failure type, status
    int count = 0;
    String list;
    for (int i = 3; i + 4 <= n; i += 4) {
      if (count) list += ", ";
      list += dtcText(resp[i], resp[i + 1], resp[i + 2], true) + " st=0x" + String(resp[i + 3], HEX);
      count++;
    }
    diagEmit("dtc", true, String(count) + " DTC(s)" + (count ? ": " + list : String("")));
    return;
  }
  if (kind == 2 && resp[0] == 0x54) { diagEmit("uds", true, what + " DTCs cleared"); return; }
  emitUdsResult(resp, n, what);
}

static void runKwp(const uint8_t *req, uint8_t len, const String &what, uint8_t kind) {
  uint8_t resp[96];
  String err;
  if (!diagKAddrExplicit && !diagKWarned) {
    diagKWarned = true;
    diagEmit("diag", false, "WARNING: no K-Line addresses set by the module profile or KLINE_CONFIG - using generic target 0x" +
                                String(diagKTarget, HEX) + " / source 0x" + String(diagKSource, HEX) + ", unverified for this module");
  }
  diagLog("kwp_req", what + " " + hexOf(req, len));
  int n = kwpExchange(diagKTarget, diagKSource, req, len, resp, sizeof(resp), err);
  if (n < 0) { diagEmit("kwp", false, what + " FAILED: " + err); return; }

  if (resp[0] == 0x7F && n >= 3) {
    diagEmit("kwp", false, what + " NEGATIVE 0x" + String(resp[2], HEX) + " " + hexOf(resp, n));
    return;
  }
  if (kind == 1 && resp[0] == 0x58 && n >= 2) {
    // 58 <count> then 3-byte records: DTC hi, DTC lo, status
    int count = resp[1];
    String list;
    int shown = 0;
    for (int i = 2; i + 2 < n && shown < count; i += 3, shown++) {
      if (shown) list += ", ";
      list += dtcText(resp[i], resp[i + 1], 0, false) + " st=0x" + String(resp[i + 2], HEX);
    }
    diagEmit("dtc", true, String(count) + " DTC(s)" + (shown ? ": " + list : String("")));
    return;
  }
  if (kind == 2 && resp[0] == 0x54) { diagEmit("kwp", true, what + " DTCs cleared"); return; }
  diagEmit("kwp", true, what + " " + hexOf(resp, n));
}

static void diagExecute(const String &cmd) {
  // Never run bus traffic while the bench is in a safety state.
  if (nano.estop || nano.fault) {
    diagEmit("diag", false, "SAFETY_BLOCK: e-stop or fault active (" + nano.faultText + ")");
    return;
  }

  int colon = cmd.indexOf(':');
  String name = colon < 0 ? cmd : cmd.substring(0, colon);
  String arg = colon < 0 ? "" : cmd.substring(colon + 1);
  arg.trim();

  uint8_t buf[256];

  if (name == "CAN_INIT" || name == "CAN_APPLY") {
    int c = arg.indexOf(',');
    long br = arg.substring(0, c < 0 ? arg.length() : c).toInt();
    int clk = (name == "CAN_INIT" && c >= 0) ? arg.substring(c + 1).toInt() : sysConfig.canClockMhz;
    if (!canInit(br, clk * 1000000L)) {
      diagEmit("can_init", false, "FAILED bitrate=" + String(br) + " clock=" + String(clk) + "MHz (unsupported value or MCP2515 not responding)");
      return;
    }
    if (name == "CAN_INIT") {
      sysConfig.canBitrate = br;
      sysConfig.canClockMhz = clk;
      saveSystemConfig(); // persist so it survives a reboot
    }
    diagEmit("can_init", true, "bitrate=" + String(br) + " clock=" + String(clk) + "MHz");
    return;
  }

  if (name == "CAN_CONFIG") {
    int c1 = arg.indexOf(','), c2 = c1 < 0 ? -1 : arg.indexOf(',', c1 + 1);
    if (c2 < 0) { diagEmit("can_cfg", false, "usage CAN_CONFIG:<txHex>,<rxHex>,<ext 0|1>[,<padHex|NONE>]"); return; }
    int c3 = arg.indexOf(',', c2 + 1);
    diagCanTx = strtoul(arg.substring(0, c1).c_str(), nullptr, 16);
    diagCanRx = strtoul(arg.substring(c1 + 1, c2).c_str(), nullptr, 16);
    diagCanExt = arg.substring(c2 + 1, c3 < 0 ? arg.length() : c3).toInt() == 1;
    diagCanAddrExplicit = true;
    if (c3 >= 0) {
      String pad = arg.substring(c3 + 1);
      pad.trim();
      pad.toUpperCase();
      if (pad == "NONE" || pad == "N") {
        isotpPadding = false;
      } else {
        isotpPadding = true;
        isotpPadByte = (uint8_t)strtoul(pad.c_str(), nullptr, 16);
      }
    }
    String padText = isotpPadding ? "pad=0x" + String(isotpPadByte, HEX) : String("no padding");
    diagEmit("can_cfg", true, "tx=0x" + String(diagCanTx, HEX) + " rx=0x" + String(diagCanRx, HEX) +
                                  (diagCanExt ? " 29-bit " : " 11-bit ") + padText);
    return;
  }

  if (name == "CAN_TX") {
    int c1 = arg.indexOf(','), c2 = c1 < 0 ? -1 : arg.indexOf(',', c1 + 1);
    if (c2 < 0) { diagEmit("can_tx", false, "usage CAN_TX:<idHex>,<ext 0|1>,<dataHex>"); return; }
    uint32_t id = strtoul(arg.substring(0, c1).c_str(), nullptr, 16);
    bool ext = arg.substring(c1 + 1, c2).toInt() == 1;
    int n = hexParse(arg.substring(c2 + 1), buf, 8);
    if (n < 0 || (!ext && id > 0x7FF) || (ext && id > 0x1FFFFFFF)) { diagEmit("can_tx", false, "BAD_ID_OR_DATA (max 8 bytes)"); return; }
    if (!canSendMessageExt(id, ext, buf, n)) { diagEmit("can_tx", false, "TX FAILED " + canErrorText()); return; }
    diagEmit("can_tx", true, "0x" + String(id, HEX) + " [" + String(n) + "] " + hexOf(buf, n));
    return;
  }

  if (name == "UDS_REQUEST") {
    int n = hexParse(arg, buf, sizeof(buf));
    if (n < 1) { diagEmit("uds", false, "BAD_HEX"); return; }
    runUds(buf, n, "REQ", (buf[0] == 0x19 && n >= 2 && buf[1] == 0x02) ? 1 : (buf[0] == 0x14 ? 2 : 0));
    return;
  }
  if (name == "UDS_SESSION") {
    int n = hexParse(arg, buf, 1);
    if (n != 1) { diagEmit("uds", false, "usage UDS_SESSION:<hex>, e.g. 01 or 03"); return; }
    uint8_t req[2] = {0x10, buf[0]};
    runUds(req, 2, "SESSION", 0);
    return;
  }
  if (name == "UDS_READ_DID") {
    int n = hexParse(arg, buf, 2);
    if (n != 2) { diagEmit("uds", false, "usage UDS_READ_DID:<4 hex digits>, e.g. F190"); return; }
    uint8_t req[3] = {0x22, buf[0], buf[1]};
    runUds(req, 3, "DID " + hexOf(buf, 2), 0);
    return;
  }
  if (name == "UDS_READ_DTC") {
    uint8_t req[3] = {0x19, 0x02, 0xFF}; // reportDTCByStatusMask, all statuses
    runUds(req, 3, "READ_DTC", 1);
    return;
  }
  if (name == "UDS_CLEAR_DTC") {
    uint8_t req[4] = {0x14, 0xFF, 0xFF, 0xFF}; // all groups
    runUds(req, 4, "CLEAR_DTC", 2);
    return;
  }
  if (name == "UDS_TESTER_PRESENT") {
    uint8_t req[2] = {0x3E, 0x00};
    runUds(req, 2, "TESTER_PRESENT", 0);
    return;
  }

  if (name == "KLINE_CONFIG") {
    int c1 = arg.indexOf(','), c2 = c1 < 0 ? -1 : arg.indexOf(',', c1 + 1);
    if (c2 < 0) { diagEmit("kline", false, "usage KLINE_CONFIG:<baud>,<targetHex>,<sourceHex>"); return; }
    long baud = arg.substring(0, c1).toInt();
    if (!klineSetBaud(baud)) { diagEmit("kline", false, "BAD_BAUD (9600 or 10400)"); return; }
    diagKTarget = (uint8_t)strtoul(arg.substring(c1 + 1, c2).c_str(), nullptr, 16);
    diagKSource = (uint8_t)strtoul(arg.substring(c2 + 1).c_str(), nullptr, 16);
    diagKAddrExplicit = true;
    diagEmit("kline", true, "baud=" + String(baud) + " target=0x" + String(diagKTarget, HEX) + " source=0x" + String(diagKSource, HEX));
    return;
  }
  if (name == "KLINE_INIT") {
    bool good = klineFastInitAddr(diagKTarget, diagKSource);
    diagEmit("kline", good, good ? "FAST_INIT OK target=0x" + String(diagKTarget, HEX) : "FAST_INIT FAILED: " + klineError);
    return;
  }
  if (name == "KLINE_5BAUD_INIT") {
    uint8_t kb1 = 0, kb2 = 0;
    bool good = klineSlowInit(diagKTarget, kb1, kb2);
    uint8_t kb[2] = {kb1, kb2};
    diagEmit("kline", good, good ? "5BAUD_INIT OK keybytes=" + hexOf(kb, 2) : "5BAUD_INIT FAILED: " + klineError);
    return;
  }

  if (name == "KWP_REQUEST") {
    int n = hexParse(arg, buf, 63);
    if (n < 1) { diagEmit("kwp", false, "BAD_HEX"); return; }
    runKwp(buf, n, "REQ", buf[0] == 0x18 ? 1 : (buf[0] == 0x14 ? 2 : 0));
    return;
  }
  if (name == "KWP_START_SESSION") { uint8_t req[2] = {0x10, 0x81}; runKwp(req, 2, "START_SESSION", 0); return; }
  if (name == "KWP_READ_DTC")      { uint8_t req[4] = {0x18, 0x00, 0xFF, 0x00}; runKwp(req, 4, "READ_DTC", 1); return; }
  if (name == "KWP_CLEAR_DTC")     { uint8_t req[3] = {0x14, 0xFF, 0x00}; runKwp(req, 3, "CLEAR_DTC", 2); return; }
  if (name == "KWP_TESTER_PRESENT"){ uint8_t req[2] = {0x3E, 0x01}; runKwp(req, 2, "TESTER_PRESENT", 0); return; }

  diagEmit("diag", false, "UNKNOWN_COMMAND " + name);
}

// ---------- entry points ----------

// Called from the BLE command handler (already PIN-authenticated, except
// DIAG_STOP). Returns true if `cmd` was a diagnostics command.
bool diagHandleCommand(const String &cmd) {
  if (cmd == "DIAG_STOP") {
    canMonitorOn = false;
    klineMonitorOn = false;
    diagTail = diagHead; // drop anything queued
    return true;
  }
  if (cmd.startsWith("CAN_MONITOR:")) {
    canMonitorOn = cmd.substring(12).toInt() == 1;
    canRingHead = canRingTail = 0;
    return true;
  }
  if (cmd.startsWith("KLINE_MONITOR:")) {
    klineMonitorOn = cmd.substring(14).toInt() == 1;
    klineMonLen = 0;
    return true;
  }

  static const char *prefixes[] = {
    "CAN_INIT", "CAN_CONFIG", "CAN_TX", "UDS_REQUEST", "UDS_SESSION", "UDS_READ_DID",
    "UDS_READ_DTC", "UDS_CLEAR_DTC", "UDS_TESTER_PRESENT", "KLINE_CONFIG", "KLINE_INIT",
    "KLINE_5BAUD_INIT", "KWP_REQUEST", "KWP_START_SESSION", "KWP_READ_DTC", "KWP_CLEAR_DTC",
    "KWP_TESTER_PRESENT"};
  String name = cmd.indexOf(':') < 0 ? cmd : cmd.substring(0, cmd.indexOf(':'));
  for (const char *p : prefixes) {
    if (name == p) {
      if (!diagPush(cmd)) diagEmit("diag", false, "BUSY (command queue full)", false);
      return true;
    }
  }
  return false;
}

// Called from loop().
void diagLoop() {
  canMonitorLoop();
  klineMonitorLoop();

  String cmd;
  if (diagPop(cmd)) diagExecute(cmd);
}

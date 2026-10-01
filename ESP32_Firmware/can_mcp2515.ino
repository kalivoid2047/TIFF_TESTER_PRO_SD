// Minimal register-level MCP2515 CAN controller driver.
//
// This talks to the MCP2515 over the existing SPI bus (shared with the SD
// card, each with its own CS line) using CAN_CS. It is a self-contained
// driver with no external library dependency, so the sketch builds without
// any extra Library Manager installs.
//
// IMPORTANT: bit timing depends on the MCP2515 breakout's crystal. 8 MHz and
// 16 MHz tables are provided (the two common cheap modules) for 125k/250k/
// 500k/1M. Pick the one that matches the crystal printed on YOUR board -
// the wrong one gives wrong bus timing (no communication, or error frames
// on a live bus). Select it with CAN_INIT:<bitrate>,<clock_mhz> from the app,
// or can_clock_mhz= in /CONFIG.INI. This driver has not been validated
// against a real CAN bus/module - see Documentation/PROJECT_STATUS.md.

#define MCP_RESET        0xC0
#define MCP_READ         0x03
#define MCP_WRITE        0x02
#define MCP_RTS          0x80
#define MCP_BITMOD       0x05

#define MCP_CANCTRL      0x0F
#define MCP_CANSTAT      0x0E
#define MCP_CNF1         0x2A
#define MCP_CNF2         0x29
#define MCP_CNF3         0x28
#define MCP_CANINTE      0x2B
#define MCP_CANINTF      0x2C
#define MCP_EFLG         0x2D
#define MCP_TEC          0x1C
#define MCP_REC          0x1D
#define MCP_TXB0CTRL     0x30
#define MCP_TXB0SIDH     0x31
#define MCP_TXB0SIDL     0x32
#define MCP_TXB0EID8     0x33
#define MCP_TXB0EID0     0x34
#define MCP_TXB0DLC      0x35
#define MCP_TXB0D0       0x36
#define MCP_RXB0CTRL     0x60
#define MCP_RXB0SIDH     0x61
#define MCP_RXB0SIDL     0x62
#define MCP_RXB0DLC      0x65
#define MCP_RXB0D0       0x66
#define MCP_RXB1CTRL     0x70

#define MCP_MODE_NORMAL  0x00
#define MCP_MODE_CONFIG  0x80
#define MCP_MODE_MASK    0xE0

bool canReady = false;
long canBitrate = 0;   // last successfully applied bitrate (0 = never)
long canOscHz = 0;     // last successfully applied oscillator

static void mcpSelect()   { digitalWrite(CAN_CS, LOW); }
static void mcpDeselect() { digitalWrite(CAN_CS, HIGH); }

static uint8_t mcpReadReg(uint8_t addr) {
  mcpSelect();
  SPI.transfer(MCP_READ);
  SPI.transfer(addr);
  uint8_t v = SPI.transfer(0x00);
  mcpDeselect();
  return v;
}

static void mcpWriteReg(uint8_t addr, uint8_t value) {
  mcpSelect();
  SPI.transfer(MCP_WRITE);
  SPI.transfer(addr);
  SPI.transfer(value);
  mcpDeselect();
}

// Changes only the bits in `mask` (atomic on the chip, unlike read-modify-write).
static void mcpBitModify(uint8_t addr, uint8_t mask, uint8_t value) {
  mcpSelect();
  SPI.transfer(MCP_BITMOD);
  SPI.transfer(addr);
  SPI.transfer(mask);
  SPI.transfer(value);
  mcpDeselect();
}

static void mcpReset() {
  mcpSelect();
  SPI.transfer(MCP_RESET);
  mcpDeselect();
  delay(10);
}

static bool mcpSetMode(uint8_t mode) {
  mcpWriteReg(MCP_CANCTRL, mode);
  delay(10);
  return (mcpReadReg(MCP_CANSTAT) & MCP_MODE_MASK) == mode;
}

// CNF1/2/3 for 8 MHz and 16 MHz oscillators (standard values for the
// 16-TQ/8-TQ layouts used by common MCP2515 libraries; CNF3 bit 7 left clear).
static bool mcpApplyBitTiming(long bitrate, long oscFreqHz) {
  uint8_t cnf1, cnf2, cnf3;

  if (oscFreqHz == 8000000) {
    switch (bitrate) {
      case 1000000: cnf1 = 0x00; cnf2 = 0x80; cnf3 = 0x00; break;
      case 500000:  cnf1 = 0x00; cnf2 = 0x90; cnf3 = 0x02; break;
      case 250000:  cnf1 = 0x00; cnf2 = 0xB1; cnf3 = 0x05; break;
      case 125000:  cnf1 = 0x01; cnf2 = 0xB1; cnf3 = 0x05; break;
      default: return false;
    }
  } else if (oscFreqHz == 16000000) {
    switch (bitrate) {
      case 1000000: cnf1 = 0x00; cnf2 = 0xD0; cnf3 = 0x02; break;
      case 500000:  cnf1 = 0x00; cnf2 = 0xF0; cnf3 = 0x06; break;
      case 250000:  cnf1 = 0x41; cnf2 = 0xF1; cnf3 = 0x05; break;
      case 125000:  cnf1 = 0x03; cnf2 = 0xF0; cnf3 = 0x06; break;
      default: return false;
    }
  } else {
    return false; // only 8 and 16 MHz are pre-computed
  }

  mcpWriteReg(MCP_CNF1, cnf1);
  mcpWriteReg(MCP_CNF2, cnf2);
  mcpWriteReg(MCP_CNF3, cnf3);
  return true;
}

// Safe to call again at runtime to change bitrate/oscillator.
bool canInit(long bitrate, long oscFreqHz) {
  pinMode(CAN_CS, OUTPUT);
  mcpDeselect();

  canReady = false;
  mcpReset();
  if (!mcpSetMode(MCP_MODE_CONFIG)) {
    Serial.println("CAN: MCP2515 not responding (config mode failed) - continuing without CAN.");
    return false;
  }

  if (!mcpApplyBitTiming(bitrate, oscFreqHz)) {
    Serial.println("CAN: unsupported bitrate/oscillator combination.");
    return false;
  }

  // Accept all messages (no filtering) - fine for a bench tool. RXB0 rolls
  // over into RXB1 when full, so bursts (ISO-TP consecutive frames) survive.
  mcpWriteReg(MCP_RXB0CTRL, 0x64);
  mcpWriteReg(MCP_RXB1CTRL, 0x60);
  mcpWriteReg(MCP_CANINTE, 0x03); // RX0IE | RX1IE

  canReady = mcpSetMode(MCP_MODE_NORMAL);
  if (canReady) {
    canBitrate = bitrate;
    canOscHz = oscFreqHz;
  }
  Serial.println(canReady ? "CAN: MCP2515 initialized." : "CAN: failed to enter normal mode.");
  return canReady;
}

// Sends one frame with an 11-bit (ext=false) or 29-bit (ext=true) ID. Returns
// false if the controller is not ready or TXB0 stayed busy (no ACK on the bus).
bool canSendMessageExt(uint32_t id, bool ext, const uint8_t *data, uint8_t len) {
  if (!canReady || len > 8) return false;

  uint32_t start = millis();
  while (mcpReadReg(MCP_TXB0CTRL) & 0x08) { // TXREQ still set from last frame
    if (millis() - start > 50) {
      mcpBitModify(MCP_TXB0CTRL, 0x08, 0x00); // abort the stuck frame
      return false;
    }
  }

  if (ext) {
    uint16_t sid = (uint16_t)((id >> 18) & 0x7FF);
    mcpWriteReg(MCP_TXB0SIDH, (uint8_t)(sid >> 3));
    mcpWriteReg(MCP_TXB0SIDL, (uint8_t)(((sid & 0x07) << 5) | 0x08 | ((id >> 16) & 0x03)));
    mcpWriteReg(MCP_TXB0EID8, (uint8_t)((id >> 8) & 0xFF));
    mcpWriteReg(MCP_TXB0EID0, (uint8_t)(id & 0xFF));
  } else {
    mcpWriteReg(MCP_TXB0SIDH, (uint8_t)(id >> 3));
    mcpWriteReg(MCP_TXB0SIDL, (uint8_t)((id & 0x07) << 5));
    mcpWriteReg(MCP_TXB0EID8, 0);
    mcpWriteReg(MCP_TXB0EID0, 0);
  }
  mcpWriteReg(MCP_TXB0DLC, len);

  for (uint8_t i = 0; i < len; i++) {
    mcpWriteReg(MCP_TXB0D0 + i, data[i]);
  }

  mcpSelect();
  SPI.transfer(MCP_RTS | 0x01); // request-to-send TXB0
  mcpDeselect();

  return true;
}

bool canSendMessage(uint32_t id, const uint8_t *data, uint8_t len) {
  return canSendMessageExt(id, false, data, len);
}

// Reads one pending frame from RXB0 or RXB1 (whichever has one).
bool canReceiveFrame(uint32_t &id, bool &ext, uint8_t *data, uint8_t &len) {
  if (!canReady) return false;

  uint8_t intf = mcpReadReg(MCP_CANINTF);
  uint8_t base, flag;
  if (intf & 0x01)      { base = 0x60; flag = 0x01; }
  else if (intf & 0x02) { base = 0x70; flag = 0x02; }
  else return false; // nothing pending

  uint8_t sidh = mcpReadReg(base + 1);
  uint8_t sidl = mcpReadReg(base + 2);
  ext = (sidl & 0x08) != 0;
  uint32_t sid = ((uint32_t)sidh << 3) | (sidl >> 5);
  if (ext) {
    uint8_t eid8 = mcpReadReg(base + 3);
    uint8_t eid0 = mcpReadReg(base + 4);
    id = (sid << 18) | ((uint32_t)(sidl & 0x03) << 16) | ((uint32_t)eid8 << 8) | eid0;
  } else {
    id = sid;
  }

  len = mcpReadReg(base + 5) & 0x0F;
  if (len > 8) len = 8;
  for (uint8_t i = 0; i < len; i++) {
    data[i] = mcpReadReg(base + 6 + i);
  }

  mcpBitModify(MCP_CANINTF, flag, 0x00); // clear only this buffer's RX flag
  return true;
}

// Original 11-bit-oriented receive; kept for existing callers.
bool canReceiveMessage(uint32_t &id, uint8_t *data, uint8_t &len) {
  bool ext;
  return canReceiveFrame(id, ext, data, len);
}

// Human-readable controller error state for the diagnostics console.
String canErrorText() {
  if (!canReady) return "CAN not ready";
  uint8_t e = mcpReadReg(MCP_EFLG);
  String s = "EFLG=0x" + String(e, HEX) + " TEC=" + String(mcpReadReg(MCP_TEC)) +
             " REC=" + String(mcpReadReg(MCP_REC));
  if (e & 0x20) s += " BUS_OFF";
  if (e & 0x10) s += " TX_ERR_PASSIVE";
  if (e & 0x08) s += " RX_ERR_PASSIVE";
  if (e & 0x01) s += " ERR_WARN";
  if (e & 0xC0) s += " RX_OVERFLOW";
  return s;
}

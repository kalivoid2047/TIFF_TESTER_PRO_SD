// Minimal register-level MCP2515 CAN controller driver.
//
// This talks to the MCP2515 over the existing SPI bus (shared with the SD
// card, each with its own CS line) using CAN_CS. It is a self-contained
// driver with no external library dependency, so the sketch builds without
// any extra Library Manager installs.
//
// IMPORTANT: the CNF1/CNF2/CNF3 bit-timing values below assume a common
// 8 MHz crystal on the MCP2515 breakout board (the most common cheap
// module). If your board uses a 16 MHz (or other) crystal, recompute these
// from the MCP2515 datasheet's bit-timing section (or an MCP2515 bit-timing
// calculator) before trusting bus timing. This driver has not been
// validated against a real CAN bus/module — see Documentation/PROJECT_STATUS.md.

#define MCP_RESET        0xC0
#define MCP_READ         0x03
#define MCP_WRITE        0x02
#define MCP_RTS          0x80

#define MCP_CANCTRL      0x0F
#define MCP_CANSTAT      0x0E
#define MCP_CNF1         0x2A
#define MCP_CNF2         0x29
#define MCP_CNF3         0x28
#define MCP_CANINTE      0x2B
#define MCP_CANINTF      0x2C
#define MCP_TXB0CTRL     0x30
#define MCP_TXB0SIDH     0x31
#define MCP_TXB0SIDL     0x32
#define MCP_TXB0DLC      0x35
#define MCP_TXB0D0       0x36
#define MCP_RXB0CTRL     0x60
#define MCP_RXB0SIDH     0x61
#define MCP_RXB0SIDL     0x62
#define MCP_RXB0DLC      0x65
#define MCP_RXB0D0       0x66

#define MCP_MODE_NORMAL  0x00
#define MCP_MODE_CONFIG  0x80
#define MCP_MODE_MASK    0xE0

bool canReady = false;

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

// Bit timing for an 8 MHz oscillator only. Extend this table (and the
// oscFreqHz check in canInit) if you need other oscillator frequencies.
static bool mcpApplyBitTiming(long bitrate, long oscFreqHz) {
  if (oscFreqHz != 8000000) {
    // Only 8 MHz is pre-computed here — add your oscillator's CNF values
    // (from the MCP2515 datasheet) if your board differs.
    return false;
  }

  uint8_t cnf1, cnf2, cnf3;
  switch (bitrate) {
    case 1000000: cnf1 = 0x00; cnf2 = 0x80; cnf3 = 0x00; break;
    case 500000:  cnf1 = 0x00; cnf2 = 0x90; cnf3 = 0x02; break;
    case 250000:  cnf1 = 0x00; cnf2 = 0xB1; cnf3 = 0x05; break;
    case 125000:  cnf1 = 0x01; cnf2 = 0xB1; cnf3 = 0x05; break;
    default: return false;
  }

  mcpWriteReg(MCP_CNF1, cnf1);
  mcpWriteReg(MCP_CNF2, cnf2);
  mcpWriteReg(MCP_CNF3, cnf3);
  return true;
}

bool canInit(long bitrate, long oscFreqHz) {
  pinMode(CAN_CS, OUTPUT);
  mcpDeselect();

  mcpReset();
  if (!mcpSetMode(MCP_MODE_CONFIG)) {
    Serial.println("CAN: MCP2515 not responding (config mode failed) - continuing without CAN.");
    canReady = false;
    return false;
  }

  if (!mcpApplyBitTiming(bitrate, oscFreqHz)) {
    Serial.println("CAN: unsupported bitrate/oscillator combination.");
    canReady = false;
    return false;
  }

  // Accept all messages on RXB0 (no filtering) - fine for a bench tool,
  // revisit if you need to share a bus with unrelated traffic.
  mcpWriteReg(MCP_RXB0CTRL, 0x60);
  mcpWriteReg(MCP_CANINTE, 0x01); // RX0IE

  canReady = mcpSetMode(MCP_MODE_NORMAL);
  Serial.println(canReady ? "CAN: MCP2515 initialized." : "CAN: failed to enter normal mode.");
  return canReady;
}

bool canSendMessage(uint32_t id, const uint8_t *data, uint8_t len) {
  if (!canReady || len > 8) return false;

  mcpWriteReg(MCP_TXB0SIDH, (uint8_t)(id >> 3));
  mcpWriteReg(MCP_TXB0SIDL, (uint8_t)((id & 0x07) << 5));
  mcpWriteReg(MCP_TXB0DLC, len);

  for (uint8_t i = 0; i < len; i++) {
    mcpWriteReg(MCP_TXB0D0 + i, data[i]);
  }

  mcpSelect();
  SPI.transfer(MCP_RTS | 0x01); // request-to-send TXB0
  mcpDeselect();

  return true;
}

bool canReceiveMessage(uint32_t &id, uint8_t *data, uint8_t &len) {
  if (!canReady) return false;

  uint8_t intf = mcpReadReg(MCP_CANINTF);
  if (!(intf & 0x01)) return false; // nothing pending in RXB0

  uint8_t sidh = mcpReadReg(MCP_RXB0SIDH);
  uint8_t sidl = mcpReadReg(MCP_RXB0SIDL);
  id = ((uint32_t)sidh << 3) | (sidl >> 5);

  len = mcpReadReg(MCP_RXB0DLC) & 0x0F;
  for (uint8_t i = 0; i < len && i < 8; i++) {
    data[i] = mcpReadReg(MCP_RXB0D0 + i);
  }

  mcpWriteReg(MCP_CANINTF, intf & ~0x01); // clear RX0IF
  return true;
}

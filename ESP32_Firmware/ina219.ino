// Optional INA219 high-side monitor for the DUT rail (I2C, address 0x40).
//
// Register-level driver using Wire only (no Library Manager dependency).
// Assumes the common breakout with a 0.1 ohm shunt and the default +/-320 mV
// range, which caps readable current at ~3.2 A; use a different shunt or PGA
// setting below for more. This is a *monitor* for the live-data preview -
// over-current cut-off stays with the Nano safety controller. Not validated
// against real hardware.

#include <Wire.h>

#define INA219_ADDR        0x40
#define INA219_REG_CONFIG  0x00
#define INA219_REG_SHUNT   0x01
#define INA219_REG_BUS     0x02
#define INA219_REG_CURRENT 0x04
#define INA219_REG_CALIB   0x05

// Cal = 0.04096 / (currentLSB * Rshunt) with currentLSB = 0.1 mA, R = 0.1 ohm.
#define INA219_CALIBRATION 4096
#define INA219_CURRENT_LSB_A 0.0001f

bool inaReady = false;
float inaBusV = 0;
float inaCurrentA = 0;
static uint32_t inaLastInitMs = 0;

static bool inaWrite16(uint8_t reg, uint16_t val) {
  Wire.beginTransmission(INA219_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(val >> 8));
  Wire.write((uint8_t)(val & 0xFF));
  return Wire.endTransmission() == 0;
}

static bool inaRead16(uint8_t reg, uint16_t &val) {
  Wire.beginTransmission(INA219_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)INA219_ADDR, (uint8_t)2) != 2) return false;
  val = (uint16_t)Wire.read() << 8;
  val |= Wire.read();
  return true;
}

void inaInit() {
  inaLastInitMs = millis();
  Wire.begin(INA_SDA, INA_SCL);
  Wire.setTimeOut(20);
  // 0x399F = power-on default (32 V bus range, PGA /8, 12-bit, continuous).
  inaReady = inaWrite16(INA219_REG_CONFIG, 0x399F) &&
             inaWrite16(INA219_REG_CALIB, INA219_CALIBRATION);
  static bool lastReported = false, reportedOnce = false;
  if (!reportedOnce || lastReported != inaReady) { // log changes, not every 5 s retry
    Serial.println(inaReady ? "INA219: found." : "INA219: not found.");
    reportedOnce = true;
    lastReported = inaReady;
  }
}

// Called from loop() at the status cadence. If the chip is missing or drops
// off the bus, retry init every 5 s rather than hammering I2C.
void inaPoll() {
  if (!inaReady) {
    if (millis() - inaLastInitMs >= 5000) inaInit();
    return;
  }

  uint16_t bus, cur;
  // Calibration register is rewritten each poll: it is cleared by a brown-out.
  if (!inaWrite16(INA219_REG_CALIB, INA219_CALIBRATION) ||
      !inaRead16(INA219_REG_BUS, bus) || !inaRead16(INA219_REG_CURRENT, cur)) {
    inaReady = false;
    return;
  }

  inaBusV = (bus >> 3) * 0.004f;
  float a = (int16_t)cur * INA219_CURRENT_LSB_A;
  inaCurrentA = a < 0 ? -a : a;
}

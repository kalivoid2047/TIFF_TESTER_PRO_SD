// Live-data preview shared by the web dashboard (/api/live, plain text) and
// the BLE Status characteristic (JSON). Kept in one tab so both surfaces
// always report the same numbers.
//
// DUT voltage/current prefer the INA219 when one is present (more accurate
// than the Nano's ADC divider/ACS712), otherwise fall back to the Nano's
// readings. Safety cut-offs always use the Nano's own readings.
//
// Position is a raw 0-3.3 V -> 0-100 % reading of POSITION_PIN; with nothing
// connected the pin is pulled down and reads 0 %, which is NOT a real
// position. Scale/offset it for your actuator's feedback sensor.

// Position input as 0..100 %.
static float livePositionPct() {
  return (analogRead(POSITION_PIN) / 4095.0f) * 100.0f;
}

static bool liveNanoOnline() {
  return lastNanoStatusMs != 0 && (millis() - lastNanoStatusMs) < 2000;
}

// "SYSTEM READY" = Nano is talking, its watchdog is supervising, no fault
// latched and the e-stop is released.
static bool liveSystemReady() {
  return liveNanoOnline() && nano.watchdog && !nano.fault && !nano.estop;
}

static float liveDutV() { return inaReady ? inaBusV : nano.dutV; }
static float liveDutA() { return inaReady ? inaCurrentA : nano.currentA; }

String liveText() {
  bool online = liveNanoOnline();
  String out;
  out += "Supply voltage - " + (online ? String(nano.supplyV, 1) + " V" : String("-- V")) + "\n";
  out += "DUT voltage - " + (online ? String(liveDutV(), 1) + " V" : String("-- V")) + "\n";
  out += "DUT current - " + (online ? String(liveDutA(), 2) + " A" : String("-- A")) + "\n";
  out += "Position - " + String(livePositionPct(), 0) + " %\n";
  out += "Temperature - " + (online ? String(nano.tempC, 1) + " C" : String("-- C")) + "\n";
  out += "\n";
  out += "Relays - R1:" + String(nano.relay ? "ON" : "OFF") +
         " R2:" + String(nano.aux[0] ? "ON" : "OFF") +
         " R3:" + String(nano.aux[1] ? "ON" : "OFF") +
         " R4:" + String(nano.aux[2] ? "ON" : "OFF") + "\n";
  out += "\n";
  out += "CAN - " + String(canReady ? "READY" : "NOT READY") +
         (canSelfTestResult == 1 ? " (self-test OK)" : (canSelfTestResult == 0 ? " (SELF-TEST FAILED)" : "")) + "\n";
  out += "K-LINE - " + String(klineReady ? "READY" : "NOT READY") +
         (klineSelfTestResult == 1 ? " (self-test OK)" : (klineSelfTestResult == 0 ? " (SELF-TEST FAILED)" : "")) + "\n";
  out += "INA219 - " + String(inaReady ? "READY" : "NOT FOUND") + "\n";
  if (nano.fault && nano.faultText.length())
    out += "SYSTEM - FAULT: " + nano.faultText + "\n";
  else
    out += "SYSTEM - " + String(liveSystemReady() ? "READY" : (online ? "NOT READY" : "NANO OFFLINE")) + "\n";
  return out;
}

String liveStatusJson() {
  String out = "{";
  out += "\"relay\":" + String(nano.relay) + ",";
  out += "\"fault\":" + String(nano.fault) + ",";
  out += "\"estop\":" + String(nano.estop) + ",";
  out += "\"watchdog\":" + String(nano.watchdog) + ",";
  out += "\"supply_v\":" + String(nano.supplyV, 2) + ",";
  out += "\"dut_v\":" + String(liveDutV(), 2) + ",";
  out += "\"current_a\":" + String(liveDutA(), 2) + ",";
  out += "\"fault_text\":\"" + nano.faultText + "\",";
  out += "\"relays\":[" + String(nano.relay ? 1 : 0) + "," + String(nano.aux[0] ? 1 : 0) + "," +
         String(nano.aux[1] ? 1 : 0) + "," + String(nano.aux[2] ? 1 : 0) + "],";
  out += "\"temp_c\":" + String(nano.tempC, 1) + ",";
  out += "\"pos_pct\":" + String(livePositionPct(), 0) + ",";
  out += "\"can\":" + String(canReady ? 1 : 0) + ",";
  out += "\"kline\":" + String(klineReady ? 1 : 0) + ",";
  out += "\"ina\":" + String(inaReady ? 1 : 0) + ",";
  out += "\"sys\":" + String(liveSystemReady() ? 1 : 0) + ",";
  out += "\"pol\":" + String(nano.relayActiveLow ? 1 : 0) + ",";
  out += "\"lim_a\":" + String(nano.limMaxA, 2) + ",";
  out += "\"lim_t\":" + String(nano.limTempC, 0) + ",";
  out += "\"aux_to\":" + String(nano.auxTimeoutS) + ",";
  out += "\"can_st\":" + String(canSelfTestResult) + ",";
  out += "\"kline_st\":" + String(klineSelfTestResult);
  out += "}";
  return out;
}

// ---------- per-test CSV log ----------
//
// While the DUT relay is energised, appends one row per second to
// /LOGS/TEST_<uptime_ms>.CSV (a new file each time the relay turns on), so a
// test leaves behind an electrical record to go with the DIAG.CSV
// communication record.
void liveLogLoop() {
  static bool wasOn = false;
  static String path;
  static uint32_t lastRow = 0;

  if (!nano.relay) { wasOn = false; return; }

  uint32_t now = millis();
  if (!wasOn) {
    wasOn = true;
    path = "/LOGS/TEST_" + String(now) + ".CSV";
    File h = SD.open(path, FILE_WRITE);
    if (h) {
      h.println("TIME_MS,SUPPLY_V,DUT_V,CURRENT_A,POWER_W,POSITION_PCT,TEMP_C,RELAYS,NANO,FAULT,MODULE,COMM");
      h.close();
    }
    lastRow = 0;
  }
  if (lastRow && now - lastRow < 1000) return;
  lastRow = now;

  float dutV = liveDutV(), dutA = liveDutA();
  File f = SD.open(path, FILE_APPEND);
  if (!f) return;
  f.print(now); f.print(",");
  f.print(nano.supplyV, 2); f.print(",");
  f.print(dutV, 2); f.print(",");
  f.print(dutA, 2); f.print(",");
  f.print(dutV * dutA, 2); f.print(",");
  f.print(livePositionPct(), 0); f.print(",");
  f.print(nano.tempC, 1); f.print(",");
  f.print(String(nano.relay ? 1 : 0) + String(nano.aux[0] ? 1 : 0) + String(nano.aux[1] ? 1 : 0) + String(nano.aux[2] ? 1 : 0)); f.print(",");
  f.print(liveNanoOnline() ? "ONLINE" : "OFFLINE"); f.print(",");
  f.print(nano.fault ? nano.faultText : String("NO")); f.print(",");
  f.print(activeModule.loaded ? activeModule.id : String("UNKNOWN")); f.print(",");
  f.println(activeModule.loaded ? activeModule.commProtocol : String(""));
  f.close();
}

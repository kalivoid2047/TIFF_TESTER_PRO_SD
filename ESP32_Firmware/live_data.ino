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
  out += "CAN - " + String(canReady ? "READY" : "NOT READY") + "\n";
  out += "K-LINE - " + String(klineReady ? "READY" : "NOT READY") + "\n";
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
  out += "\"sys\":" + String(liveSystemReady() ? 1 : 0);
  out += "}";
  return out;
}

// Consumes the [TESTS]/[SERVICE] sections of a module profile and runs the
// subset of tests that are meaningful with the sensors this bench
// currently has (Nano-reported DUT voltage/current). Tests that need
// injector/coil driver hardware not yet built (see
// Documentation/ROADMAP.md Phase 1) are reported as not implemented rather
// than faked with a made-up result.
//
// [SERVICE] routines (turbo_actuator_calibration, dpf_forced_regeneration,
// injector_pilot_learn, scv_learn, ...) are intentionally never executed by
// this firmware — see runServiceRoutine() below and Documentation/SRS.md.
// This function still *consumes* the section (reads and reports its flags)
// so the module schema is fully parsed, it just refuses to act on it:
// doing so safely requires a per-module-validated implementation
// (including OEM security-access/seed-key handling) that does not exist
// here, consistent with the existing firmware's caution about sending
// arbitrary UDS routines to a vehicle/module.

static bool iniFlag(const String &data, const String &key) {
  int pos = data.indexOf(key + "=");
  if (pos < 0) return false;
  int end = data.indexOf('\n', pos);
  String v = data.substring(pos + key.length() + 1, end < 0 ? data.length() : end);
  v.trim();
  return v == "1";
}

static float iniFloat(const String &data, const String &key, float fallback) {
  int pos = data.indexOf(key + "=");
  if (pos < 0) return fallback;
  int end = data.indexOf('\n', pos);
  String v = data.substring(pos + key.length() + 1, end < 0 ? data.length() : end);
  v.trim();
  return v.length() ? v.toFloat() : fallback;
}

static String iniString(const String &data, const String &key) {
  int pos = data.indexOf(key + "=");
  if (pos < 0) return "";
  int end = data.indexOf('\n', pos);
  String v = data.substring(pos + key.length() + 1, end < 0 ? data.length() : end);
  v.trim();
  return v;
}

// Extracts the text of one `[SECTION]` block (up to the next `[` header or
// EOF) so same-named keys in different sections (e.g. "enabled=" under both
// [TEST_INJECTOR] and [TEST_COIL]) don't collide — iniFlag()/iniFloat()/
// iniString() above are otherwise section-agnostic, flat substring scans.
static String iniSection(const String &data, const String &header) {
  int pos = data.indexOf(header);
  if (pos < 0) return "";
  int start = data.indexOf('\n', pos);
  if (start < 0) return "";
  start += 1;
  int end = data.indexOf("\n[", start);
  return data.substring(start, end < 0 ? data.length() : end);
}

bool loadActiveModule(const String &id) {
  String path = modulePathFromId(id);
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  String data;
  while (f.available()) data += (char)f.read();
  f.close();

  activeModule = ModuleProfile();
  activeModule.id = id;
  activeModule.minVoltageV = iniFloat(data, "min_voltage_v", 0);
  activeModule.maxVoltageV = iniFloat(data, "max_voltage_v", 0);
  activeModule.maxCurrentA = iniFloat(data, "max_current_a", 0);
  activeModule.resMinOhm = iniFloat(data, "min_ohm", 0);
  activeModule.resMaxOhm = iniFloat(data, "max_ohm", 0);
  activeModule.testResistance = iniFlag(data, "resistance");
  activeModule.testShortToGround = iniFlag(data, "short_to_ground");
  activeModule.testPositionSweep = iniFlag(data, "position_sweep");
  activeModule.testActuatorMovement = iniFlag(data, "actuator_movement");
  activeModule.testCurrentMonitor = iniFlag(data, "current_monitor");
  activeModule.testPassFail = iniFlag(data, "pass_fail");

  String injSection = iniSection(data, "[TEST_INJECTOR]");
  activeModule.testInjectorEnabled = iniFlag(injSection, "enabled");
  activeModule.injectorDefaultPulseWidthMs = iniFloat(injSection, "default_pulse_width_ms", 0);
  activeModule.injectorMaxPulseWidthMs = iniFloat(injSection, "max_pulse_width_ms", 0);
  activeModule.injectorDefaultDurationS = (int)iniFloat(injSection, "default_test_duration_s", 0);

  String coilSection = iniSection(data, "[TEST_COIL]");
  activeModule.testCoilEnabled = iniFlag(coilSection, "enabled");
  activeModule.coilDefaultDwellMs = iniFloat(coilSection, "default_dwell_ms", 0);
  activeModule.coilMaxDwellMs = iniFloat(coilSection, "max_dwell_ms", 0);
  activeModule.coilDefaultDurationS = (int)iniFloat(coilSection, "default_test_duration_s", 0);

  activeModule.serviceTurboCal = iniFlag(data, "turbo_actuator_calibration");
  activeModule.serviceDpfRegen = iniFlag(data, "dpf_forced_regeneration");
  activeModule.serviceInjectorLearn = iniFlag(data, "injector_pilot_learn");
  activeModule.serviceScvLearn = iniFlag(data, "scv_learn");
  activeModule.commProtocol = iniString(data, "protocol");
  activeModule.commBitrate = (long)iniFloat(data, "bitrate", 0);

  // Diagnostic addressing from [COMMUNICATION] (all optional; defaults are
  // the generic OBD-II CAN IDs / functional K-Line addresses).
  String commSection = iniSection(data, "[COMMUNICATION]");
  if (iniString(commSection, "can_tx_id").length())
    activeModule.canTxId = strtoul(iniString(commSection, "can_tx_id").c_str(), nullptr, 0);
  if (iniString(commSection, "can_rx_id").length())
    activeModule.canRxId = strtoul(iniString(commSection, "can_rx_id").c_str(), nullptr, 0);
  activeModule.canExtended = iniFlag(commSection, "can_extended");
  if (iniFloat(commSection, "kline_baud", 0) > 0)
    activeModule.klineBaud = (long)iniFloat(commSection, "kline_baud", 10400);
  if (iniString(commSection, "kline_target").length())
    activeModule.klineTarget = (uint8_t)strtoul(iniString(commSection, "kline_target").c_str(), nullptr, 0);
  if (iniString(commSection, "kline_source").length())
    activeModule.klineSource = (uint8_t)strtoul(iniString(commSection, "kline_source").c_str(), nullptr, 0);
  activeModule.loaded = true;
  diagApplyModule(); // diag_engine.ino - push addressing/bitrate to the engine

  return true;
}

// No RTC on this board yet — timestamps are uptime-based. Replace with a
// real RTC/NTP timestamp once one is added to the hardware (see
// Documentation/ROADMAP.md).
static String isoTimestamp() {
  return "uptime_ms:" + String(millis());
}

TestResult runResistanceTest() {
  TestResult r;
  r.testType = "resistance";
  if (!activeModule.loaded || !activeModule.testResistance) {
    r.pass = false;
    r.response = "NOT_ENABLED_FOR_MODULE";
    return r;
  }
  readNano();
  if (nano.currentA < 0.01) {
    r.pass = false;
    r.response = "NO_CURRENT_FLOW - ensure DUT power is on before running this test";
    return r;
  }
  float ohms = nano.dutV / nano.currentA;
  r.pass = (ohms >= activeModule.resMinOhm && ohms <= activeModule.resMaxOhm);
  r.response = "measured_ohm=" + String(ohms, 2);
  return r;
}

TestResult runShortToGroundTest() {
  TestResult r;
  r.testType = "short_to_ground";
  if (!activeModule.loaded || !activeModule.testShortToGround) {
    r.pass = false;
    r.response = "NOT_ENABLED_FOR_MODULE";
    return r;
  }
  readNano();
  bool shorted = nano.currentA > (activeModule.maxCurrentA * 0.9);
  r.pass = !shorted;
  r.response = shorted ? "current_near_limit=" + String(nano.currentA, 2) : "OK";
  return r;
}

TestResult runCurrentMonitorTest() {
  TestResult r;
  r.testType = "current_monitor";
  if (!activeModule.loaded || !activeModule.testCurrentMonitor) {
    r.pass = false;
    r.response = "NOT_ENABLED_FOR_MODULE";
    return r;
  }
  readNano();
  r.pass = nano.currentA <= activeModule.maxCurrentA;
  r.response = "current_a=" + String(nano.currentA, 2);
  return r;
}

TestResult runUnavailableHardwareTest(const String &type) {
  TestResult r;
  r.testType = type;
  r.pass = false;
  r.response = "NOT_IMPLEMENTED - requires injector/coil driver hardware not yet built (see Documentation/ROADMAP.md Phase 1)";
  return r;
}

// Reads and reports [SERVICE] flags but never executes them — see the file
// header comment above for why.
TestResult runServiceRoutine(const String &routineName) {
  TestResult r;
  r.testType = "service:" + routineName;
  r.pass = false;
  r.response = "NOT_IMPLEMENTED - OEM service routines require a validated, per-module implementation "
               "(including OEM security access) and are intentionally not executed by this firmware. "
               "See Documentation/SRS.md.";
  return r;
}

void appendResultToReport(const TestResult &r) {
  if (!SD.exists("/REPORTS")) SD.mkdir("/REPORTS");

  String moduleId = activeModule.loaded ? activeModule.id : String("UNKNOWN");
  String reportPath = "/REPORTS/" + moduleId + ".jsonl";
  File f = SD.open(reportPath, FILE_APPEND);
  if (f) {
    String line = "{";
    line += "\"module_id\":\"" + moduleId + "\",";
    line += "\"test_type\":\"" + r.testType + "\",";
    line += "\"result\":\"" + String(r.pass ? "PASS" : "FAIL") + "\",";
    line += "\"response\":\"" + r.response + "\",";
    line += "\"timestamp\":\"" + isoTimestamp() + "\"";
    line += "}";
    f.println(line);
    f.close();
  }

  logLine("/LOGS/tests.log",
          r.testType + " -> " + (r.pass ? "PASS" : "FAIL") + " (" + r.response + ")");
}

void handleModuleSelect() {
  String id = server.arg("id");
  id.trim();
  if (!id.length()) {
    server.send(400, "text/plain", "Missing id.");
    return;
  }
  if (!loadActiveModule(id)) {
    server.send(404, "text/plain", "Module not found: " + id);
    return;
  }
  server.send(200, "text/plain", "Active module: " + id);
}

void handleModuleActive() {
  if (!activeModule.loaded) {
    server.send(200, "text/plain", "No module selected.");
    return;
  }
  String out = "id=" + activeModule.id +
               " min_v=" + String(activeModule.minVoltageV, 1) +
               " max_v=" + String(activeModule.maxVoltageV, 1) +
               " max_a=" + String(activeModule.maxCurrentA, 1) +
               " resistance_test=" + String(activeModule.testResistance) +
               " short_to_ground_test=" + String(activeModule.testShortToGround) +
               " position_sweep_test=" + String(activeModule.testPositionSweep) +
               " actuator_movement_test=" + String(activeModule.testActuatorMovement) +
               " current_monitor_test=" + String(activeModule.testCurrentMonitor) +
               " injector_test=" + String(activeModule.testInjectorEnabled) +
               " injector_max_pulse_width_ms=" + String(activeModule.injectorMaxPulseWidthMs, 1) +
               " coil_test=" + String(activeModule.testCoilEnabled) +
               " coil_max_dwell_ms=" + String(activeModule.coilMaxDwellMs, 1) +
               " comm_protocol=" + activeModule.commProtocol;
  server.send(200, "text/plain", out);
}

void handleTestRun() {
  String test = server.arg("test");
  TestResult r;

  if (test == "resistance") r = runResistanceTest();
  else if (test == "short_to_ground") r = runShortToGroundTest();
  else if (test == "current_monitor") r = runCurrentMonitorTest();
  else if (test == "position_sweep") r = runUnavailableHardwareTest("position_sweep");
  else if (test == "actuator_movement") r = runUnavailableHardwareTest("actuator_movement");
  else if (test.startsWith("service:")) r = runServiceRoutine(test.substring(8));
  else {
    server.send(400, "text/plain", "Unknown test: " + test);
    return;
  }

  appendResultToReport(r);

  String out = r.testType + "=" + (r.pass ? "PASS" : "FAIL") + " " + r.response;
  server.send(200, "text/plain", out);
}

void handleReportsList() {
  File dir = SD.open("/REPORTS");
  if (!dir || !dir.isDirectory()) {
    server.send(500, "text/plain", "REPORTS directory unavailable.");
    return;
  }
  String out = "REPORTS\n";
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) out += String(f.name()) + "\n";
    f.close();
    f = dir.openNextFile();
  }
  server.send(200, "text/plain", out);
}

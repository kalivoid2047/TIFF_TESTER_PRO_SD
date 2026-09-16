/*
  TIFF_TESTER_PRO_SD - Arduino Nano Safety / I/O Controller

  FAIL-SAFE PRINCIPLE:
    - Relay OFF at boot.
    - Relay OFF when ESP32 heartbeat is lost.
    - Relay OFF on emergency stop.
    - Relay OFF on undervoltage/overvoltage.
    - Relay OFF on overcurrent.
    - Nano continuously supervises the ESP32.

  UART commands from ESP32:
    HEARTBEAT
    POWER_ON
    POWER_OFF
    STATUS
    RESET_FAULT
    CAL_SUPPLY,<actual_voltage>          calibrate supply divider ratio
    CAL_DUT,<actual_voltage>             calibrate DUT divider ratio
    CAL_CURRENT_ZERO                     capture current-sensor zero point (DUT off)
    CAL_CURRENT_SCALE,<actual_current_a> calibrate current-sensor V/A scale
    SET_RELAY_POLARITY,<0|1>             0=active-high (default), 1=active-low relay module
    RESET_CALIBRATION                    revert to firmware-default constants
    GET_CONFIG                           report current calibration/polarity state
    INJ_TEST,<channel>,<pulse_width_ms>,<duration_s>   start injector test window
    COIL_TEST,<channel>,<dwell_ms>,<duration_s>        start coil test window
    TEST_STOP                                          end the current test window

  IMPORTANT re: INJ_TEST/COIL_TEST — there is no injector/coil driver
  hardware on this board yet (Documentation/ROADMAP.md Phase 1), so these
  commands do NOT pulse anything. What they DO is exactly what
  Documentation/API_PROTOCOL_SPEC.md §4.3 asks for ahead of that hardware
  landing: accept the request, clamp the requested pulse-width/dwell to a
  hard-coded max regardless of input, run safetyOK() continuously for the
  duration of the test window (not just once before starting, the way
  relayOnSafe() does for the DUT relay), and report TEST_STATUS lines. Once
  Phase 1 hardware exists, the actual driver GPIO write slots into
  startTestWindow()/stopTestWindow() below.

  IMPORTANT:
    Calibration and relay-polarity commands above give you a way to tune
    this firmware to your actual hardware WITHOUT recompiling, but nobody
    has run them against a real bench here. You must still perform the
    calibration procedure yourself with a known-good reference (a trusted
    multimeter for voltage, a trusted ammeter/known load for current) and
    confirm relay polarity against your actual relay module before
    connecting an automotive DUT. See Documentation/PROJECT_STATUS.md.
*/

#include <Arduino.h>
#include <avr/wdt.h>
#include <EEPROM.h>

// ---------- Pins ----------
#define RELAY_PIN        4
#define ESTOP_PIN        3
#define BUZZER_PIN       6
#define STATUS_LED       13

#define DUT_VOLT_PIN     A0
#define SUPPLY_VOLT_PIN  A1
#define CURRENT_PIN      A2
#define TEMP_PIN         A3

// ---------- Limits ----------
const float MIN_SUPPLY_V = 11.0;
const float MAX_SUPPLY_V = 15.0;
const float MAX_DUT_V    = 15.0;
const float MAX_CURRENT_A = 5.0;

// Hard caps for INJ_TEST/COIL_TEST, enforced regardless of what the ESP32
// requests (Documentation/API_PROTOCOL_SPEC.md §4.3, SRS FR-INJ-4/FR-COIL-4).
// Match the max_pulse_width_ms/max_dwell_ms defaults proposed in SRS §6.1.
const float MAX_PULSE_WIDTH_MS = 8.0;
const float MAX_DWELL_MS = 8.0;
const unsigned long MAX_TEST_DURATION_S = 30;

// Adjust these for your actual analog circuits.
const float ADC_REF = 5.0;
const float ADC_COUNTS = 1023.0;

// Firmware-default calibration constants. These are the values used until
// a CAL_* command (see above) persists a bench-measured value to EEPROM.
// They are placeholders, not calibrated numbers — replace via the CAL_*
// commands rather than editing these before trusting a reading.
const float SUPPLY_DIVIDER = 4.0;
const float DUT_DIVIDER = 4.0;
const float CURRENT_ZERO_V = 2.50;   // ACS712 5A: ~2.5V zero point
const float CURRENT_V_PER_A = 0.185; // ACS712 5A: ~185 mV/A

// ---------- EEPROM-backed calibration/config ----------
#define EE_MAGIC_ADDR           0
#define EE_MAGIC_VALUE          0xA5
#define EE_SUPPLY_DIV_ADDR      1   // float, 4 bytes
#define EE_DUT_DIV_ADDR         5   // float, 4 bytes
#define EE_CUR_ZERO_ADDR        9   // float, 4 bytes
#define EE_CUR_SCALE_ADDR       13  // float, 4 bytes
#define EE_RELAY_POLARITY_ADDR  17  // byte

float supplyDividerCal = SUPPLY_DIVIDER;
float dutDividerCal = DUT_DIVIDER;
float currentZeroCal = CURRENT_ZERO_V;
float currentScaleCal = CURRENT_V_PER_A;
bool relayActiveLow = false; // false = GPIO HIGH energizes (existing default behavior)
bool calibrated = false;

void loadCalibration() {
  if (EEPROM.read(EE_MAGIC_ADDR) == EE_MAGIC_VALUE) {
    EEPROM.get(EE_SUPPLY_DIV_ADDR, supplyDividerCal);
    EEPROM.get(EE_DUT_DIV_ADDR, dutDividerCal);
    EEPROM.get(EE_CUR_ZERO_ADDR, currentZeroCal);
    EEPROM.get(EE_CUR_SCALE_ADDR, currentScaleCal);
    relayActiveLow = EEPROM.read(EE_RELAY_POLARITY_ADDR) == 1;
    calibrated = true;
  } else {
    supplyDividerCal = SUPPLY_DIVIDER;
    dutDividerCal = DUT_DIVIDER;
    currentZeroCal = CURRENT_ZERO_V;
    currentScaleCal = CURRENT_V_PER_A;
    relayActiveLow = false;
    calibrated = false;
  }
}

void saveCalibration() {
  EEPROM.put(EE_SUPPLY_DIV_ADDR, supplyDividerCal);
  EEPROM.put(EE_DUT_DIV_ADDR, dutDividerCal);
  EEPROM.put(EE_CUR_ZERO_ADDR, currentZeroCal);
  EEPROM.put(EE_CUR_SCALE_ADDR, currentScaleCal);
  EEPROM.write(EE_RELAY_POLARITY_ADDR, relayActiveLow ? 1 : 0);
  EEPROM.write(EE_MAGIC_ADDR, EE_MAGIC_VALUE);
  calibrated = true;
}

// ---------- Watchdog / heartbeat ----------
const unsigned long HEARTBEAT_TIMEOUT_MS = 2000;
const unsigned long STATUS_INTERVAL_MS = 500;
unsigned long lastHeartbeat = 0;
unsigned long lastStatus = 0;

// ---------- State ----------
bool relayOn = false;
bool fault = false;
String faultText = "";
String rxLine;

float supplyV = 0;
float dutV = 0;
float currentA = 0;

// ---------- Injector/coil test window state ----------
// See the file header for why this is safety-supervision-only for now (no
// driver hardware to actually pulse).
const unsigned long TEST_STATUS_INTERVAL_MS = 500;
bool testActive = false;
String testType = "";        // "INJ" or "COIL"
int testChannel = 0;
unsigned long testStartMs = 0;
unsigned long testDurationS = 0;
unsigned long lastTestStatus = 0;

float adcVoltage(uint8_t pin) {
  return (analogRead(pin) * ADC_REF) / ADC_COUNTS;
}

float readSupplyVoltage() {
  return adcVoltage(SUPPLY_VOLT_PIN) * supplyDividerCal;
}

float readDutVoltage() {
  return adcVoltage(DUT_VOLT_PIN) * dutDividerCal;
}

float readCurrent() {
  float v = adcVoltage(CURRENT_PIN);
  float a = (v - currentZeroCal) / currentScaleCal;
  if (a < 0) a = -a;
  return a;
}

// Maps a logical "energize/de-energize" request to the correct physical
// GPIO level for the configured relay polarity, so relayActiveLow can be
// changed at runtime (via SET_RELAY_POLARITY) without touching any other
// safety logic below.
void writeRelayPhysical(bool energize) {
  bool pinHigh = relayActiveLow ? !energize : energize;
  digitalWrite(RELAY_PIN, pinHigh ? HIGH : LOW);
}

void relayOff(const char *reason = "") {
  writeRelayPhysical(false);
  relayOn = false;

  if (reason && strlen(reason)) {
    fault = true;
    faultText = reason;
    digitalWrite(BUZZER_PIN, HIGH);
  }
}

void clearFault() {
  fault = false;
  faultText = "";
  digitalWrite(BUZZER_PIN, LOW);
}

bool safetyOK() {
  supplyV = readSupplyVoltage();
  dutV = readDutVoltage();
  currentA = readCurrent();

  if (digitalRead(ESTOP_PIN) == LOW) {
    relayOff("ESTOP");
    return false;
  }

  if (supplyV < MIN_SUPPLY_V) {
    relayOff("UNDERVOLTAGE");
    return false;
  }

  if (supplyV > MAX_SUPPLY_V) {
    relayOff("OVERVOLTAGE");
    return false;
  }

  if (dutV > MAX_DUT_V) {
    relayOff("DUT_OVERVOLTAGE");
    return false;
  }

  if (currentA > MAX_CURRENT_A) {
    relayOff("OVERCURRENT");
    return false;
  }

  return true;
}

void relayOnSafe() {
  clearFault();

  if (!safetyOK()) {
    relayOff(faultText.c_str());
    return;
  }

  writeRelayPhysical(true);
  relayOn = true;

  // Recheck after energizing.
  delay(20);
  if (!safetyOK()) {
    relayOff(faultText.c_str());
  }
}

// status: "TESTING" | "DONE" | "STOPPED" | "FAULT"
// result: "PASS" | "FAIL" | "" (unknown/not yet evaluated)
void sendTestStatus(const char *status, const char *result, const char *faultReason) {
  unsigned long elapsedS = (millis() - testStartMs) / 1000UL;
  Serial.print("TEST_STATUS,");
  Serial.print(testType);
  Serial.print(",");
  Serial.print(testChannel);
  Serial.print(",");
  Serial.print(status);
  Serial.print(",");
  Serial.print(elapsedS);
  Serial.print(",");
  Serial.print(currentA, 2);
  Serial.print(",");
  Serial.print(result);
  Serial.print(",");
  Serial.println(faultReason);
}

// Starts a supervised test window. `paramMs` is the requested
// pulse-width/dwell (clamped to the hard cap before this is called);
// `durationS` is clamped to MAX_TEST_DURATION_S. Refuses to start under the
// same safetyOK() gate relayOnSafe() uses for the DUT relay.
void startTestWindow(const char *type, int channel, unsigned long durationS) {
  if (!safetyOK()) {
    testType = type;
    testChannel = channel;
    testStartMs = millis();
    sendTestStatus("FAULT", "", faultText.c_str());
    return;
  }

  testActive = true;
  testType = type;
  testChannel = channel;
  testStartMs = millis();
  testDurationS = min(durationS, MAX_TEST_DURATION_S);
  lastTestStatus = 0; // force an immediate status line below
  sendTestStatus("TESTING", "", "");
}

void stopTestWindow(const char *status, const char *result, const char *faultReason) {
  if (!testActive) return;
  testActive = false;
  sendTestStatus(status, result, faultReason);
}

void sendStatus() {
  Serial.print("STATUS,");
  Serial.print(relayOn ? 1 : 0);
  Serial.print(",");
  Serial.print(fault ? 1 : 0);
  Serial.print(",");
  Serial.print(digitalRead(ESTOP_PIN) == LOW ? 1 : 0);
  Serial.print(",");
  Serial.print(1); // Nano watchdog supervision active
  Serial.print(",");
  Serial.print(supplyV, 2);
  Serial.print(",");
  Serial.print(dutV, 2);
  Serial.print(",");
  Serial.print(currentA, 2);
  Serial.print(",");
  Serial.println(faultText);
}

void sendConfig() {
  Serial.print("CONFIG,");
  Serial.print(supplyDividerCal, 4);
  Serial.print(",");
  Serial.print(dutDividerCal, 4);
  Serial.print(",");
  Serial.print(currentZeroCal, 4);
  Serial.print(",");
  Serial.print(currentScaleCal, 4);
  Serial.print(",");
  Serial.print(relayActiveLow ? 1 : 0);
  Serial.print(",");
  Serial.println(calibrated ? 1 : 0);
}

void processCommand(String cmd) {
  cmd.trim();

  if (cmd == "HEARTBEAT") {
    lastHeartbeat = millis();
    return;
  }

  if (cmd == "POWER_ON") {
    lastHeartbeat = millis();
    relayOnSafe();
    return;
  }

  if (cmd == "POWER_OFF") {
    lastHeartbeat = millis();
    relayOff();
    clearFault();
    stopTestWindow("STOPPED", "", "");
    return;
  }

  if (cmd == "RESET_FAULT") {
    lastHeartbeat = millis();
    clearFault();
    return;
  }

  if (cmd == "STATUS") {
    sendStatus();
    return;
  }

  if (cmd.startsWith("CAL_SUPPLY,")) {
    lastHeartbeat = millis();
    float actual = cmd.substring(String("CAL_SUPPLY,").length()).toFloat();
    float raw = adcVoltage(SUPPLY_VOLT_PIN);
    if (actual > 0.0 && raw > 0.05) {
      supplyDividerCal = actual / raw;
      saveCalibration();
    }
    sendConfig();
    return;
  }

  if (cmd.startsWith("CAL_DUT,")) {
    lastHeartbeat = millis();
    float actual = cmd.substring(String("CAL_DUT,").length()).toFloat();
    float raw = adcVoltage(DUT_VOLT_PIN);
    if (actual > 0.0 && raw > 0.05) {
      dutDividerCal = actual / raw;
      saveCalibration();
    }
    sendConfig();
    return;
  }

  if (cmd == "CAL_CURRENT_ZERO") {
    lastHeartbeat = millis();
    currentZeroCal = adcVoltage(CURRENT_PIN);
    saveCalibration();
    sendConfig();
    return;
  }

  if (cmd.startsWith("CAL_CURRENT_SCALE,")) {
    lastHeartbeat = millis();
    float actual = cmd.substring(String("CAL_CURRENT_SCALE,").length()).toFloat();
    float v = adcVoltage(CURRENT_PIN);
    float delta = v - currentZeroCal;
    if (delta < 0) delta = -delta;
    if (actual > 0.01 && delta > 0.001) {
      currentScaleCal = delta / actual;
      saveCalibration();
    }
    sendConfig();
    return;
  }

  if (cmd.startsWith("SET_RELAY_POLARITY,")) {
    lastHeartbeat = millis();
    relayOff("POLARITY_CHANGE"); // force safe state under the OLD polarity first
    int v = cmd.substring(String("SET_RELAY_POLARITY,").length()).toInt();
    relayActiveLow = (v == 1);
    writeRelayPhysical(false); // re-assert de-energized under the NEW polarity
    saveCalibration();
    sendConfig();
    return;
  }

  if (cmd == "RESET_CALIBRATION") {
    lastHeartbeat = millis();
    relayOff("POLARITY_CHANGE");
    supplyDividerCal = SUPPLY_DIVIDER;
    dutDividerCal = DUT_DIVIDER;
    currentZeroCal = CURRENT_ZERO_V;
    currentScaleCal = CURRENT_V_PER_A;
    relayActiveLow = false;
    writeRelayPhysical(false);
    calibrated = false;
    EEPROM.write(EE_MAGIC_ADDR, 0x00);
    sendConfig();
    return;
  }

  if (cmd == "GET_CONFIG") {
    sendConfig();
    return;
  }

  // INJ_TEST,<channel>,<pulse_width_ms>,<duration_s> /
  // COIL_TEST,<channel>,<dwell_ms>,<duration_s> — see file header. Per
  // API_PROTOCOL_SPEC.md §4.3, a pulse-width/dwell above the hard-coded max
  // is rejected outright (the window never starts) rather than silently
  // clamped, since there's no driver output yet to actually apply a
  // clamped value to.
  if (cmd.startsWith("INJ_TEST,") || cmd.startsWith("COIL_TEST,")) {
    lastHeartbeat = millis();
    bool isInjector = cmd.startsWith("INJ_TEST,");
    String params = cmd.substring(cmd.indexOf(',') + 1);

    int c1 = params.indexOf(',');
    int c2 = c1 < 0 ? -1 : params.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) return; // malformed, ignore

    int channel = params.substring(0, c1).toInt();
    float paramMs = params.substring(c1 + 1, c2).toFloat();
    unsigned long durationS = (unsigned long)params.substring(c2 + 1).toInt();

    const char *type = isInjector ? "INJ" : "COIL";
    float cap = isInjector ? MAX_PULSE_WIDTH_MS : MAX_DWELL_MS;
    if (paramMs < 0 || paramMs > cap) {
      testType = type;
      testChannel = channel;
      testStartMs = millis();
      sendTestStatus("FAULT", "", "PARAM_EXCEEDS_MAX");
      return;
    }

    startTestWindow(type, channel, durationS);
    return;
  }

  if (cmd == "TEST_STOP") {
    lastHeartbeat = millis();
    stopTestWindow("STOPPED", "", "");
    return;
  }
}

void readSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n') {
      processCommand(rxLine);
      rxLine = "";
    } else if (c != '\r' && rxLine.length() < 80) {
      rxLine += c;
    }
  }
}

void setup() {
  // Load calibration/polarity before touching the relay pin, so the very
  // first output write already respects whatever polarity was last saved.
  loadCalibration();

  // Ensure outputs are safe before enabling anything.
  digitalWrite(RELAY_PIN, relayActiveLow ? HIGH : LOW); // de-energized, pre-pinMode
  pinMode(RELAY_PIN, OUTPUT);
  writeRelayPhysical(false);

  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  Serial.begin(115200);

  // Hardware watchdog: if Nano firmware itself locks up, reset the Nano.
  wdt_enable(WDTO_2S);

  lastHeartbeat = millis();
}

void loop() {
  wdt_reset();
  readSerialCommands();

  unsigned long now = millis();

  // Independent safety supervision.
  supplyV = readSupplyVoltage();
  dutV = readDutVoltage();
  currentA = readCurrent();

  if (relayOn) {
    if (!safetyOK()) {
      relayOff(faultText.c_str());
    }

    if (now - lastHeartbeat > HEARTBEAT_TIMEOUT_MS) {
      relayOff("ESP32_HEARTBEAT_TIMEOUT");
    }
  }

  if (digitalRead(ESTOP_PIN) == LOW) {
    relayOff("ESTOP");
  }

  // Continuous supervision for the duration of an injector/coil test window
  // — not just a check before it starts, matching relayOnSafe()'s
  // recheck-after-energize pattern but held for the whole window.
  if (testActive) {
    if (!safetyOK()) {
      stopTestWindow("FAULT", "", faultText.c_str());
    } else if (now - lastHeartbeat > HEARTBEAT_TIMEOUT_MS) {
      stopTestWindow("FAULT", "", "ESP32_HEARTBEAT_TIMEOUT");
    } else if ((now - testStartMs) / 1000UL >= testDurationS) {
      stopTestWindow("DONE", "",
          "NOT_IMPLEMENTED - no injector/coil driver hardware, safety-only supervision");
    } else if (now - lastTestStatus >= TEST_STATUS_INTERVAL_MS) {
      sendTestStatus("TESTING", "", "");
      lastTestStatus = now;
    }
  }

  if (now - lastStatus >= STATUS_INTERVAL_MS) {
    sendStatus();
    lastStatus = now;
  }

  digitalWrite(STATUS_LED, relayOn ? HIGH : LOW);
}

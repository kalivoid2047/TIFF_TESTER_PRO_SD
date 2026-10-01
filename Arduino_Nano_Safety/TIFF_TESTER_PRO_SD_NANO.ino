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
    SET_RELAY_POLARITY,<0|1>             0=active-high, 1=active-low relay module (default)
    RESET_CALIBRATION                    revert to firmware-default constants
    GET_CONFIG                           report current calibration/polarity state
    INJ_TEST,<channel>,<pulse_width_ms>,<duration_s>   start injector test window
    COIL_TEST,<channel>,<dwell_ms>,<duration_s>        start coil test window
    TEST_STOP                                          end the current test window
    RELAY,<n>,<0|1>                      switch relay n (1=DUT, 2-4=auxiliary)
    RELAY_TEST,<n>                       cycle relay n ON/OFF 3x and report
    RELAY_TEST_STOP                      abort a running relay test
    RELAY_PULSE,<n>,<ms>                 aux relay n (2-4) on for ms (50-5000); re-send to hold
    SET_LIMITS,<minV>,<maxV>,<maxA>      TIGHTEN the protection window (never loosen past the hard caps)
    RESET_LIMITS                         back to the hard caps
    SET_TEMP_LIMIT,<degC>                over-temperature trip (0 = disabled, saved to EEPROM)
    SET_AUX_TIMEOUT,<seconds>            auto-off for aux relays 2-4 (0 = none, saved to EEPROM)

  Relays: 1 = DUT relay (D4, full safetyOK() gating, unchanged behavior),
  2-4 = auxiliary bench relays (D5/D7/D8). Auxiliary relays follow the same
  fail-safe rules as the DUT relay: OFF at boot, OFF on e-stop, any latched
  fault, ESP32 heartbeat loss, POWER_OFF and polarity change. They share the
  relay polarity setting. Only relay 1 has electrical feedback (DUT voltage)
  so only its RELAY_TEST can report PASS/FAIL; relays 2-4 report ACTUATED
  (commanded cycles completed, verify by click/load) because nothing on
  this board can observe them - unless you wire per-relay feedback inputs
  and set AUX_FEEDBACK_ENABLED 1 (see below), in which case relays 2-4 also
  report PASS/FAIL.

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
#define AUX_RELAY2_PIN   5
#define AUX_RELAY3_PIN   7
#define AUX_RELAY4_PIN   8
#define STATUS_LED       13

// Optional per-relay feedback for aux relays 2-4. Wire a sense signal that
// follows the relay (e.g. an auxiliary contact, or an optocoupler across the
// load) to these pins and set AUX_FEEDBACK_ENABLED to 1. With it disabled
// (default) nothing on this board can observe relays 2-4, so RELAY_TEST can
// only report ACTUATED, never PASS/FAIL.
#define AUX_FEEDBACK_ENABLED  0
#define AUX_FB_ACTIVE_LOW     1   // pin reads LOW when the sense contact is closed
const uint8_t AUX_FB_PINS[3] = {2, 11, 12};

#define DUT_VOLT_PIN     A0
#define SUPPLY_VOLT_PIN  A1
#define CURRENT_PIN      A2
#define TEMP_PIN         A3

// A3 is read as an LM35-style sensor (10 mV/degC, 0 degC = 0 V). Placeholder
// like the other analog constants - change for TMP36/NTC hardware.
const float TEMP_C_PER_V = 100.0;

// ---------- Limits ----------
const float MIN_SUPPLY_V = 11.0;
const float MAX_SUPPLY_V = 15.0;
const float MAX_DUT_V    = 15.0;
const float MAX_CURRENT_A = 5.0;

// Runtime limits. The consts above are the HARD caps; a module profile may only
// tighten them (SET_LIMITS clamps into these ranges), never loosen. They are
// RAM-only and revert to the hard caps on reset, which is the permissive
// direction - the ESP32 re-sends the active module's limits periodically.
float limMinSupplyV = MIN_SUPPLY_V;
float limMaxSupplyV = MAX_SUPPLY_V;
float limMaxCurrentA = MAX_CURRENT_A;
float limMaxTempC = 0;        // over-temperature trip; 0 = disabled (EEPROM-backed)
uint16_t auxTimeoutS = 0;     // aux relay auto-off; 0 = none (EEPROM-backed)

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

// Protection settings (separate magic from the calibration block above, so an
// older EEPROM image without them is left untouched).
#define EE_SETTINGS_MAGIC_ADDR  24
#define EE_SETTINGS_MAGIC_VALUE 0x5C
#define EE_TEMP_LIMIT_ADDR      25  // float, 4 bytes
#define EE_AUX_TIMEOUT_ADDR     29  // uint16, 2 bytes

float supplyDividerCal = SUPPLY_DIVIDER;
float dutDividerCal = DUT_DIVIDER;
float currentZeroCal = CURRENT_ZERO_V;
float currentScaleCal = CURRENT_V_PER_A;
// Default is ACTIVE-LOW (GPIO LOW energizes, HIGH = off) to match the relay
// module in use. Active-low is also the safer unconfigured default: a pulled-up
// or floating input keeps the relay released. A value saved to EEPROM by
// SET_RELAY_POLARITY overrides this. Applies to all four relays.
const bool RELAY_DEFAULT_ACTIVE_LOW = true;
bool relayActiveLow = RELAY_DEFAULT_ACTIVE_LOW;
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
    relayActiveLow = RELAY_DEFAULT_ACTIVE_LOW;
    calibrated = false;
  }
}

void loadSettings() {
  if (EEPROM.read(EE_SETTINGS_MAGIC_ADDR) != EE_SETTINGS_MAGIC_VALUE) return;
  float t;
  uint16_t a;
  EEPROM.get(EE_TEMP_LIMIT_ADDR, t);
  EEPROM.get(EE_AUX_TIMEOUT_ADDR, a);
  // Reject garbage (NaN/out of range) rather than trusting it.
  if (t == t && t >= 0 && t <= 150) limMaxTempC = t;
  if (a <= 3600) auxTimeoutS = a;
}

void saveSettings() {
  EEPROM.put(EE_TEMP_LIMIT_ADDR, limMaxTempC);
  EEPROM.put(EE_AUX_TIMEOUT_ADDR, auxTimeoutS);
  EEPROM.write(EE_SETTINGS_MAGIC_ADDR, EE_SETTINGS_MAGIC_VALUE);
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

// ---------- Auxiliary relays (2-4) and relay test ----------
const uint8_t AUX_RELAY_PINS[3] = {AUX_RELAY2_PIN, AUX_RELAY3_PIN, AUX_RELAY4_PIN};
bool auxOn[3] = {false, false, false};
unsigned long auxOffAt[3] = {0, 0, 0}; // auto-off deadline (millis), 0 = none
float tempC = 0;

const uint8_t RELAY_TEST_CYCLES = 3;
const unsigned long RELAY_TEST_STEP_MS = 600; // ON phase and OFF phase length
bool relayTestActive = false;
uint8_t relayTestRelay = 0;
uint8_t relayTestStep = 0;        // 0..(2*cycles-1): even=ON phase, odd=OFF phase
unsigned long relayTestStepStart = 0;
bool relayTestFailed = false;
String relayTestDetail = "";

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

void writeAuxPhysical(uint8_t idx, bool energize) {
  bool pinHigh = relayActiveLow ? !energize : energize;
  digitalWrite(AUX_RELAY_PINS[idx], pinHigh ? HIGH : LOW);
  auxOn[idx] = energize;
  if (!energize) auxOffAt[idx] = 0;
}

// Is the sense input for aux relay idx (0..2 = relays 2..4) reporting "on"?
// Only meaningful when AUX_FEEDBACK_ENABLED is 1.
bool auxFeedbackOn(uint8_t idx) {
#if AUX_FEEDBACK_ENABLED
  bool low = digitalRead(AUX_FB_PINS[idx]) == LOW;
  return AUX_FB_ACTIVE_LOW ? low : !low;
#else
  (void)idx;
  return false;
#endif
}

void auxAllOff() {
  for (uint8_t i = 0; i < 3; i++) writeAuxPhysical(i, false);
}

void abortRelayTest(const char *detail);

void relayOff(const char *reason = "") {
  writeRelayPhysical(false);
  relayOn = false;

  if (reason && strlen(reason)) {
    // A fault drops every output, not just the DUT relay.
    auxAllOff();
    if (relayTestActive) abortRelayTest(reason);
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
  tempC = adcVoltage(TEMP_PIN) * TEMP_C_PER_V;

  if (digitalRead(ESTOP_PIN) == LOW) {
    relayOff("ESTOP");
    return false;
  }

  if (supplyV < limMinSupplyV) {
    relayOff("UNDERVOLTAGE");
    return false;
  }

  if (supplyV > limMaxSupplyV) {
    relayOff("OVERVOLTAGE");
    return false;
  }

  if (dutV > MAX_DUT_V) {
    relayOff("DUT_OVERVOLTAGE");
    return false;
  }

  if (currentA > limMaxCurrentA) {
    relayOff("OVERCURRENT");
    return false;
  }

  if (limMaxTempC > 0 && tempC > limMaxTempC) {
    relayOff("OVERTEMPERATURE");
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
// status: "RUNNING" | "DONE" | "ABORTED"
// result: "PASS" | "FAIL" (relay 1, DUT-voltage feedback) | "ACTUATED"
//         (relays 2-4, no feedback) | "" while running/aborted
void sendRelayTest(const char *status, uint8_t cycle, const char *result, const char *detail) {
  Serial.print("RELAY_TEST,");
  Serial.print(relayTestRelay);
  Serial.print(",");
  Serial.print(status);
  Serial.print(",");
  Serial.print(cycle);
  Serial.print(",");
  Serial.print(result);
  Serial.print(",");
  Serial.println(detail);
}

void abortRelayTest(const char *detail) {
  if (!relayTestActive) return;
  relayTestActive = false;
  sendRelayTest("ABORTED", relayTestStep / 2, "", detail);
}

// Aux relays (2-4) are bench outputs, not the DUT, so they are NOT subject
// to the supply undervoltage/current limits (that would make them untestable
// on a bench without 12 V). They still refuse on e-stop, a latched fault, or
// supply overvoltage.
bool auxMaySwitchOn() {
  if (digitalRead(ESTOP_PIN) == LOW) { relayOff("ESTOP"); return false; }
  if (fault) return false;
  if (supplyV > limMaxSupplyV) { relayOff("OVERVOLTAGE"); return false; }
  return true;
}

// n: 1 = DUT relay, 2-4 = auxiliary. Returns false if refused.
bool setRelay(uint8_t n, bool on) {
  if (n == 1) {
    if (on) {
      relayOnSafe();
      return relayOn;
    }
    writeRelayPhysical(false);
    relayOn = false;
    return true;
  }
  if (n < 2 || n > 4) return false;
  if (on && !auxMaySwitchOn()) return false;
  writeAuxPhysical(n - 2, on);
  // Optional auto-off so a forgotten aux relay can't stay on indefinitely.
  auxOffAt[n - 2] = (on && auxTimeoutS) ? ((millis() + auxTimeoutS * 1000UL) | 1) : 0;
  return true;
}

bool relayIsOn(uint8_t n) {
  return n == 1 ? relayOn : auxOn[n - 2];
}

void startRelayTest(uint8_t n) {
  relayTestRelay = n;
  relayTestStep = 0;
  if (n < 1 || n > 4) { sendRelayTest("ABORTED", 0, "", "BAD_RELAY"); return; }
  if (testActive) { sendRelayTest("ABORTED", 0, "", "TEST_WINDOW_ACTIVE"); return; }
  if (relayTestActive) { sendRelayTest("ABORTED", 0, "", "ALREADY_RUNNING"); return; }
  if (relayIsOn(n)) { sendRelayTest("ABORTED", 0, "", "RELAY_ALREADY_ON"); return; }
  if (n == 1 ? !safetyOK() : !auxMaySwitchOn()) {
    sendRelayTest("ABORTED", 0, "", faultText.length() ? faultText.c_str() : "NOT_SAFE");
    return;
  }
  relayTestActive = true;
  relayTestFailed = false;
  relayTestDetail = "";
  relayTestStepStart = 0; // 0 => step not yet entered
  sendRelayTest("RUNNING", 0, "", "");
}

// Called from loop(); non-blocking so the watchdog/heartbeat/e-stop checks
// keep running for the whole test.
void runRelayTest(unsigned long now) {
  if (!relayTestActive) return;

  bool onPhase = (relayTestStep % 2) == 0;

  if (relayTestStepStart == 0) {
    // Enter the step.
    if (onPhase) {
      if (!setRelay(relayTestRelay, true)) {
        abortRelayTest(faultText.length() ? faultText.c_str() : "REFUSED");
        return;
      }
    } else {
      setRelay(relayTestRelay, false);
    }
    relayTestStepStart = now ? now : 1;
    return;
  }

  if (now - relayTestStepStart < RELAY_TEST_STEP_MS) return;

  // End of step: aux relays use their feedback input when one is wired.
  if (AUX_FEEDBACK_ENABLED && relayTestRelay >= 2) {
    bool fb = auxFeedbackOn(relayTestRelay - 2);
    if (onPhase && !fb && !relayTestFailed) {
      relayTestFailed = true;
      relayTestDetail = "NO_FEEDBACK_WHEN_ON";
    }
    if (!onPhase && fb && !relayTestFailed) {
      relayTestFailed = true;
      relayTestDetail = "FEEDBACK_WHEN_OFF";
    }
  }

  // End of step: for the DUT relay, use DUT voltage as feedback.
  if (relayTestRelay == 1) {
    if (onPhase && dutV < 0.5f * supplyV && !relayTestFailed) {
      relayTestFailed = true;
      relayTestDetail = "NO_DUT_VOLTAGE_WHEN_ON";
    }
    if (!onPhase && dutV > 2.0f && !relayTestFailed) {
      relayTestFailed = true;
      relayTestDetail = "DUT_VOLTAGE_WHEN_OFF";
    }
  }

  relayTestStep++;
  relayTestStepStart = 0;

  if (relayTestStep >= RELAY_TEST_CYCLES * 2) {
    relayTestActive = false;
    setRelay(relayTestRelay, false);
    bool hasFeedback = relayTestRelay == 1 || (AUX_FEEDBACK_ENABLED && relayTestRelay >= 2);
    const char *result = hasFeedback ? (relayTestFailed ? "FAIL" : "PASS") : "ACTUATED";
    sendRelayTest("DONE", RELAY_TEST_CYCLES, result, relayTestDetail.c_str());
  } else if (relayTestStep % 2 == 0) {
    sendRelayTest("RUNNING", relayTestStep / 2, "", "");
  }
}

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

// EXT,<relay1>,<relay2>,<relay3>,<relay4>,<tempC>,<activeLow> - kept as its own line so
// the existing STATUS line format (and its consumers) is unchanged.
void sendExtStatus() {
  Serial.print("EXT,");
  Serial.print(relayOn ? 1 : 0);
  for (uint8_t i = 0; i < 3; i++) {
    Serial.print(",");
    Serial.print(auxOn[i] ? 1 : 0);
  }
  Serial.print(",");
  Serial.print(tempC, 1);
  Serial.print(",");
  Serial.println(relayActiveLow ? 1 : 0);
}

// LIMITS,<minV>,<maxV>,<maxA>,<maxTempC>,<auxTimeoutS>
void sendLimits() {
  Serial.print("LIMITS,");
  Serial.print(limMinSupplyV, 2);
  Serial.print(",");
  Serial.print(limMaxSupplyV, 2);
  Serial.print(",");
  Serial.print(limMaxCurrentA, 2);
  Serial.print(",");
  Serial.print(limMaxTempC, 1);
  Serial.print(",");
  Serial.println(auxTimeoutS);
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
    auxAllOff();
    abortRelayTest("POWER_OFF");
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
    sendExtStatus();
    sendLimits();
    return;
  }

  // SET_LIMITS,<minV>,<maxV>,<maxA> - a module profile may only TIGHTEN the
  // protection window: values are clamped into the hard caps, never beyond.
  if (cmd.startsWith("SET_LIMITS,")) {
    lastHeartbeat = millis();
    String p = cmd.substring(11);
    int c1 = p.indexOf(',');
    int c2 = c1 < 0 ? -1 : p.indexOf(',', c1 + 1);
    if (c2 >= 0) {
      float mn = p.substring(0, c1).toFloat();
      float mx = p.substring(c1 + 1, c2).toFloat();
      float ma = p.substring(c2 + 1).toFloat();
      if (mn < MIN_SUPPLY_V) mn = MIN_SUPPLY_V;
      if (mx > MAX_SUPPLY_V) mx = MAX_SUPPLY_V;
      if (ma > MAX_CURRENT_A) ma = MAX_CURRENT_A;
      if (mn < mx && ma >= 0.1) { // reject nonsense instead of applying it
        limMinSupplyV = mn;
        limMaxSupplyV = mx;
        limMaxCurrentA = ma;
      }
    }
    sendLimits();
    return;
  }

  if (cmd == "RESET_LIMITS") {
    lastHeartbeat = millis();
    limMinSupplyV = MIN_SUPPLY_V;
    limMaxSupplyV = MAX_SUPPLY_V;
    limMaxCurrentA = MAX_CURRENT_A;
    sendLimits();
    return;
  }

  // SET_TEMP_LIMIT,<degC>: 0 disables, otherwise 30..150. Saved to EEPROM.
  if (cmd.startsWith("SET_TEMP_LIMIT,")) {
    lastHeartbeat = millis();
    float t = cmd.substring(15).toFloat();
    if (t <= 0) t = 0;
    else if (t < 30) t = 30;
    else if (t > 150) t = 150;
    limMaxTempC = t;
    saveSettings();
    sendLimits();
    return;
  }

  // SET_AUX_TIMEOUT,<seconds>: 0 = none, else 1..3600. Applies to relays
  // switched on afterwards. Saved to EEPROM.
  if (cmd.startsWith("SET_AUX_TIMEOUT,")) {
    lastHeartbeat = millis();
    long sec = cmd.substring(16).toInt();
    if (sec < 0) sec = 0;
    if (sec > 3600) sec = 3600;
    auxTimeoutS = (uint16_t)sec;
    saveSettings();
    sendLimits();
    return;
  }

  // RELAY_PULSE,<n>,<ms>: momentary "hold to energise" for aux relays 2-4. The
  // relay drops by itself when the deadline passes, so a lost "release"
  // message can't leave it on; the app re-sends while the button is held.
  if (cmd.startsWith("RELAY_PULSE,")) {
    lastHeartbeat = millis();
    String p = cmd.substring(12);
    int c = p.indexOf(',');
    if (c < 0) return;
    int n = p.substring(0, c).toInt();
    long ms = p.substring(c + 1).toInt();
    if (relayTestActive || n < 2 || n > 4) return;
    if (ms < 50) ms = 50;
    if (ms > 5000) ms = 5000;
    if (!auxMaySwitchOn()) return;
    writeAuxPhysical(n - 2, true);
    auxOffAt[n - 2] = (millis() + (unsigned long)ms) | 1;
    return;
  }

  // RELAY,<n>,<0|1>
  if (cmd.startsWith("RELAY,")) {
    lastHeartbeat = millis();
    String params = cmd.substring(6);
    int c = params.indexOf(',');
    if (c < 0) return;
    int n = params.substring(0, c).toInt();
    int v = params.substring(c + 1).toInt();
    if (relayTestActive) return; // a relay test owns the outputs until it ends
    setRelay(n, v == 1);
    sendExtStatus();
    return;
  }

  if (cmd.startsWith("RELAY_TEST,")) {
    lastHeartbeat = millis();
    startRelayTest((uint8_t)cmd.substring(11).toInt());
    return;
  }

  if (cmd == "RELAY_TEST_STOP") {
    lastHeartbeat = millis();
    if (relayTestActive) {
      abortRelayTest("STOPPED");
      setRelay(relayTestRelay, false);
    }
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
    auxAllOff();
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
    relayActiveLow = RELAY_DEFAULT_ACTIVE_LOW;
    writeRelayPhysical(false);
    auxAllOff();
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
  loadSettings();

  // Ensure outputs are safe before enabling anything.
  digitalWrite(RELAY_PIN, relayActiveLow ? HIGH : LOW); // de-energized, pre-pinMode
  pinMode(RELAY_PIN, OUTPUT);
  writeRelayPhysical(false);
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(AUX_RELAY_PINS[i], relayActiveLow ? HIGH : LOW); // de-energized, pre-pinMode
    pinMode(AUX_RELAY_PINS[i], OUTPUT);
    writeAuxPhysical(i, false);
  }

  pinMode(ESTOP_PIN, INPUT_PULLUP);
#if AUX_FEEDBACK_ENABLED
  for (uint8_t i = 0; i < 3; i++) pinMode(AUX_FB_PINS[i], INPUT_PULLUP);
#endif
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
  tempC = adcVoltage(TEMP_PIN) * TEMP_C_PER_V;

  if (relayOn) {
    if (!safetyOK()) {
      relayOff(faultText.c_str());
    }
  }

  // Over-temperature covers aux-only operation too (safetyOK() only runs for
  // the DUT relay / test windows).
  if (limMaxTempC > 0 && tempC > limMaxTempC && (relayOn || auxOn[0] || auxOn[1] || auxOn[2])) {
    relayOff("OVERTEMPERATURE");
  }

  // Aux relay auto-off / pulse expiry.
  for (uint8_t i = 0; i < 3; i++) {
    if (auxOn[i] && auxOffAt[i] && (long)(now - auxOffAt[i]) >= 0) {
      writeAuxPhysical(i, false);
      sendExtStatus(); // let the ESP32/app see the release promptly
    }
  }

  // Heartbeat supervision covers every output, including aux-only operation.
  if (relayOn || auxOn[0] || auxOn[1] || auxOn[2]) {
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
  runRelayTest(now);

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
    sendExtStatus();
    sendLimits();
    lastStatus = now;
  }

  digitalWrite(STATUS_LED, relayOn ? HIGH : LOW);
}

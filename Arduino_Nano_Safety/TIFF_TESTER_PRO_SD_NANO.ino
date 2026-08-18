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

  IMPORTANT:
    Calibrate ADC scaling and current-sensor conversion for the actual
    voltage divider/current sensor before connecting an automotive DUT.
*/

#include <Arduino.h>
#include <avr/wdt.h>

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

// Adjust these for your actual analog circuits.
const float ADC_REF = 5.0;
const float ADC_COUNTS = 1023.0;

// Example voltage divider ratio.
// Replace with the real divider ratio used on your board.
const float SUPPLY_DIVIDER = 4.0;
const float DUT_DIVIDER = 4.0;

// Example current sensor:
// ACS712 5A version is ~185 mV/A with ~2.5 V zero point.
// Replace these values for the actual sensor.
const float CURRENT_ZERO_V = 2.50;
const float CURRENT_V_PER_A = 0.185;

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

float adcVoltage(uint8_t pin) {
  return (analogRead(pin) * ADC_REF) / ADC_COUNTS;
}

float readSupplyVoltage() {
  return adcVoltage(SUPPLY_VOLT_PIN) * SUPPLY_DIVIDER;
}

float readDutVoltage() {
  return adcVoltage(DUT_VOLT_PIN) * DUT_DIVIDER;
}

float readCurrent() {
  float v = adcVoltage(CURRENT_PIN);
  float a = (v - CURRENT_ZERO_V) / CURRENT_V_PER_A;
  if (a < 0) a = -a;
  return a;
}

void relayOff(const char *reason = "") {
  digitalWrite(RELAY_PIN, LOW); // relay module must be wired so LOW = OFF
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

  digitalWrite(RELAY_PIN, HIGH);
  relayOn = true;

  // Recheck after energizing.
  delay(20);
  if (!safetyOK()) {
    relayOff(faultText.c_str());
  }
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
  // Ensure outputs are safe before enabling anything.
  digitalWrite(RELAY_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);

  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
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

  if (now - lastStatus >= STATUS_INTERVAL_MS) {
    sendStatus();
    lastStatus = now;
  }

  digitalWrite(STATUS_LED, relayOn ? HIGH : LOW);
}

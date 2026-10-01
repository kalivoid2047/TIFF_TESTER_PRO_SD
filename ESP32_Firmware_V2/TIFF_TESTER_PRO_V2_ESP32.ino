/*
  TIFF TESTER PRO V2.2
  ESP32-WROOM-DA - Bench Tester / Diagnostic Platform
  Communication: Bluetooth Classic ONLY
  Wi-Fi removed to reduce firmware size and complexity.

  Arduino IDE 1.8.19
  ESP32 core 3.x

  IMPORTANT HARDWARE ASSUMPTIONS:
  - 12 V bench version.
  - ESP32 GPIO34 receives a PROTECTED voltage-divider signal, never raw 12 V.
  - INA219 is on I2C address 0x40 unless changed below.
  - Nano I2C address = 0x12.
  - Relay inputs are active LOW.
  - MOSFET inputs are active HIGH.
  - MCP2515 clock below defaults to 8 MHz. Change to 16 MHz if your board has a 16 MHz crystal.
  - K-Line UART is an interface only; full KWP/ISO9141 protocol is not implemented here.
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <BluetoothSerial.h>
#include <Adafruit_INA219.h>
#include "esp_task_wdt.h"

// -------------------- PINS --------------------
#define I2C_SDA       21
#define I2C_SCL       22

#define SPI_SCK       18
#define SPI_MISO      19
#define SPI_MOSI      23

#define MCP_CS        5
#define SD_CS         13

#define KLINE_RX      16
#define KLINE_TX      17

#define SUPPLY_ADC    34
#define POSITION_ADC  36
#define TEMP_ADC      39

#define DUT_RELAY     27

// -------------------- SETTINGS --------------------
#define BT_NAME       "TIFF_TESTER_V2"
#define NANO_ADDR     0x12
#define INA_ADDR      0x40

#define SUPPLY_DIVIDER_RATIO 4.7037f

#define SYSTEM_MIN_V 11.0f
#define SYSTEM_MAX_V 15.0f
#define SYSTEM_MAX_CURRENT 5.0f
#define OVERCURRENT_MS 150UL

#define LOG_INTERVAL_MS 1000UL
#define NANO_HEARTBEAT_INTERVAL 500UL
#define NANO_TIMEOUT_MS 1500UL

#define FW_VERSION "2.2"

// -------------------- NANO PROTOCOL --------------------
#define NANO_GET_STATUS 0x01
#define NANO_ALL_OFF    0x02
#define NANO_HEARTBEAT  0x03

#define NANO_R1_ON  0x10
#define NANO_R1_OFF 0x11
#define NANO_R2_ON  0x12
#define NANO_R2_OFF 0x13
#define NANO_R3_ON  0x14
#define NANO_R3_OFF 0x15
#define NANO_R4_ON  0x16
#define NANO_R4_OFF 0x17

#define NANO_M1_ON  0x20
#define NANO_M1_OFF 0x21
#define NANO_M2_ON  0x22
#define NANO_M2_OFF 0x23

// -------------------- OBJECTS --------------------
BluetoothSerial SerialBT;
Adafruit_INA219 ina219(INA_ADDR);

// -------------------- STATE --------------------
bool sdReady = false;
bool inaReady = false;
bool nanoOnline = false;
bool canReady = false;
bool klineReady = false;

bool faultActive = false;
String faultReason = "";

bool testRunning = false;
bool pretestPassed = false;

String selectedModule = "";
String activeLogFile = "";
String activeReportFile = "";

unsigned long lastHeartbeat = 0;
unsigned long lastLiveLog = 0;
unsigned long overCurrentStart = 0;
unsigned long testStartMillis = 0;
unsigned long testCounter = 0;

// -------------------- HELPERS --------------------
void printBoth(const String &s) {
  Serial.println(s);
  if (SerialBT.hasClient()) SerialBT.println(s);
}

void btReply(const String &s) {
  SerialBT.println(s);
  Serial.println("[BT] " + s);
}

bool validFilename(String name) {
  name.trim();
  if (name.length() < 1 || name.length() > 40) return false;
  if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 ||
      name.indexOf("..") >= 0 || name.indexOf('|') >= 0) return false;
  return true;
}

String normalizeModuleFilename(String s) {
  s.trim();
  if (!s.endsWith(".INI") && !s.endsWith(".ini")) s += ".INI";
  return s;
}

String modulePath(const String &filename) {
  return "/MODULES/" + normalizeModuleFilename(filename);
}

String getField(const String &data, const String &key) {
  String target = key + "=";
  int p = data.indexOf(target);
  if (p < 0) return "";
  p += target.length();

  int e = data.indexOf('\n', p);
  if (e < 0) e = data.length();

  String value = data.substring(p, e);
  value.trim();
  return value;
}

bool hasField(const String &data, const String &key) {
  return getField(data, key).length() > 0;
}

bool parseLongValue(const String &s, long &v) {
  if (s.length() == 0) return false;
  char *endp;
  v = strtol(s.c_str(), &endp, 10);
  return endp != s.c_str() && *endp == '\0';
}

bool parseFloatValue(const String &s, float &v) {
  if (s.length() == 0) return false;
  char *endp;
  v = strtof(s.c_str(), &endp);
  return endp != s.c_str() && *endp == '\0';
}

void ensureDirectories() {
  if (!sdReady) return;
  if (!SD.exists("/MODULES")) SD.mkdir("/MODULES");
  if (!SD.exists("/LOGS")) SD.mkdir("/LOGS");
  if (!SD.exists("/REPORTS")) SD.mkdir("/REPORTS");
}

String makeSessionName() {
  testCounter++;
  return "/LOGS/TEST_" + String(testCounter) + ".CSV";
}

// -------------------- SENSORS --------------------
float readSupplyVoltage() {
  uint16_t raw = analogRead(SUPPLY_ADC);
  float adcV = (raw / 4095.0f) * 3.3f;
  return adcV * SUPPLY_DIVIDER_RATIO;
}

float readPositionPercent() {
  uint16_t raw = analogRead(POSITION_ADC);
  return (raw / 4095.0f) * 100.0f;
}

float readTemperatureC() {
  // Placeholder until exact temperature sensor type is confirmed.
  // Reports the ADC voltage as a diagnostic-derived value.
  uint16_t raw = analogRead(TEMP_ADC);
  float v = (raw / 4095.0f) * 3.3f;
  return v;
}

float readCurrentA() {
  if (!inaReady) return 0.0f;
  return ina219.getCurrent_mA() / 1000.0f;
}

float readDutVoltage() {
  if (!inaReady) return 0.0f;
  return ina219.getBusVoltage_V();
}

float readPowerW() {
  if (!inaReady) return 0.0f;
  return ina219.getPower_mW() / 1000.0f;
}

// -------------------- NANO --------------------
bool nanoCommand(uint8_t command) {
  Wire.beginTransmission(NANO_ADDR);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

uint8_t nanoStatus() {
  Wire.beginTransmission(NANO_ADDR);
  Wire.write(NANO_GET_STATUS);
  if (Wire.endTransmission(false) != 0) return 0xFF;

  if (Wire.requestFrom(NANO_ADDR, (uint8_t)1) != 1) return 0xFF;
  return Wire.read();
}

void updateNanoStatus() {
  uint8_t s = nanoStatus();
  nanoOnline = (s != 0xFF);
}

void sendNanoHeartbeat() {
  if (nanoCommand(NANO_HEARTBEAT)) {
    nanoOnline = true;
  } else {
    nanoOnline = false;
  }
}

void allOutputsOff() {
  digitalWrite(DUT_RELAY, LOW);
  nanoCommand(NANO_ALL_OFF);
}

void nanoOutputCommand(uint8_t cmd, const String &label) {
  if (faultActive) {
    btReply("BLOCKED|FAULT_ACTIVE=" + faultReason);
    return;
  }

  if (!nanoCommand(cmd)) {
    nanoOnline = false;
    btReply("ERROR|NANO_OFFLINE");
    return;
  }

  btReply("OK|" + label);
}

// -------------------- MCP2515 BASIC CHECK --------------------
void canWriteReg(uint8_t reg, uint8_t value) {
  digitalWrite(MCP_CS, LOW);
  SPI.transfer(0x02);
  SPI.transfer(reg);
  SPI.transfer(value);
  digitalWrite(MCP_CS, HIGH);
}

uint8_t canReadReg(uint8_t reg) {
  digitalWrite(MCP_CS, LOW);
  SPI.transfer(0x03);
  SPI.transfer(reg);
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(MCP_CS, HIGH);
  return v;
}

bool initCANHardware() {
  pinMode(MCP_CS, OUTPUT);
  digitalWrite(MCP_CS, HIGH);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, MCP_CS);

  // Reset command
  digitalWrite(MCP_CS, LOW);
  SPI.transfer(0xC0);
  digitalWrite(MCP_CS, HIGH);
  delay(10);

  uint8_t stat = canReadReg(0x0E);
  // MCP2515 CANSTAT after reset should normally indicate configuration mode.
  canReady = ((stat & 0xE0) == 0x80);

  return canReady;
}

// -------------------- VALIDATION --------------------
bool validateModuleFile(const String &filename, String &error, String &summary) {
  error = "";
  summary = "";

  if (!sdReady) {
    error = "SD_NOT_READY";
    return false;
  }

  if (!validFilename(filename)) {
    error = "INVALID_FILENAME";
    return false;
  }

  String path = modulePath(filename);
  if (!SD.exists(path)) {
    error = "MODULE_NOT_FOUND";
    return false;
  }

  File f = SD.open(path, FILE_READ);
  if (!f) {
    error = "MODULE_OPEN_FAILED";
    return false;
  }

  String data;
  while (f.available()) data += char(f.read());
  f.close();

  const char *required[] = {
    "VEHICLE", "MODULE", "COMM",
    "VOLTAGE_MIN", "VOLTAGE_MAX",
    "CURRENT_MAX", "TEST_TIMEOUT"
  };

  for (uint8_t i = 0; i < 7; i++) {
    if (!hasField(data, required[i])) {
      error = String("MISSING_") + required[i];
      return false;
    }
  }

  String comm = getField(data, "COMM");
  comm.toUpperCase();

  if (comm != "CAN" && comm != "KLINE") {
    error = "UNSUPPORTED_COMM";
    return false;
  }

  float vmin, vmax, imax;
  long timeout;

  if (!parseFloatValue(getField(data, "VOLTAGE_MIN"), vmin) ||
      !parseFloatValue(getField(data, "VOLTAGE_MAX"), vmax) ||
      !parseFloatValue(getField(data, "CURRENT_MAX"), imax) ||
      !parseLongValue(getField(data, "TEST_TIMEOUT"), timeout)) {
    error = "INVALID_NUMERIC_FIELD";
    return false;
  }

  if (vmin < 0 || vmax > SYSTEM_MAX_V || vmin >= vmax) {
    error = "VOLTAGE_LIMIT_INVALID";
    return false;
  }

  if (vmin < SYSTEM_MIN_V) {
    error = "VOLTAGE_MIN_BELOW_SYSTEM_LIMIT";
    return false;
  }

  if (imax <= 0 || imax > SYSTEM_MAX_CURRENT) {
    error = "CURRENT_LIMIT_INVALID";
    return false;
  }

  if (timeout < 1 || timeout > 600) {
    error = "TEST_TIMEOUT_INVALID";
    return false;
  }

  if (comm == "CAN") {
    long speed, tx, rx;

    if (!parseLongValue(getField(data, "CAN_SPEED"), speed) ||
        !parseLongValue(getField(data, "CAN_TX"), tx) ||
        !parseLongValue(getField(data, "CAN_RX"), rx)) {
      error = "CAN_FIELDS_INVALID";
      return false;
    }

    if (speed != 125000 && speed != 250000 &&
        speed != 500000 && speed != 1000000) {
      error = "CAN_SPEED_UNSUPPORTED";
      return false;
    }

    if (tx < 0 || tx > 0x7FF || rx < 0 || rx > 0x7FF) {
      error = "CAN_ID_INVALID";
      return false;
    }

    summary = "COMM=CAN,CAN_SPEED=" + String(speed) +
              ",CAN_TX=0x" + String(tx, HEX) +
              ",CAN_RX=0x" + String(rx, HEX);
  }

  if (comm == "KLINE") {
    long baud = 10400;
    if (hasField(data, "KLINE_BAUD")) {
      if (!parseLongValue(getField(data, "KLINE_BAUD"), baud)) {
        error = "KLINE_BAUD_INVALID";
        return false;
      }
    }

    if (baud != 9600 && baud != 10400) {
      error = "KLINE_BAUD_UNSUPPORTED";
      return false;
    }

    summary = "COMM=KLINE,KLINE_BAUD=" + String(baud);
  }

  summary += ",VOLTAGE_MIN=" + String(vmin, 2);
  summary += ",VOLTAGE_MAX=" + String(vmax, 2);
  summary += ",CURRENT_MAX=" + String(imax, 2);
  summary += ",TEST_TIMEOUT=" + String(timeout);

  return true;
}

bool markModuleValidated(const String &filename) {
  String path = modulePath(filename);
  File in = SD.open(path, FILE_READ);
  if (!in) return false;

  String data;
  while (in.available()) data += char(in.read());
  in.close();

  // Remove old validation metadata if present.
  String lines = "";
  int start = 0;
  while (start < data.length()) {
    int end = data.indexOf('\n', start);
    if (end < 0) end = data.length();

    String line = data.substring(start, end);
    line.trim();

    if (!line.startsWith("VALIDATED=") &&
        !line.startsWith("VALIDATED_BY=") &&
        !line.startsWith("VALIDATED_AT_MS=")) {
      if (line.length()) {
        lines += line + "\n";
      }
    }
    start = end + 1;
  }

  lines += "VALIDATED=YES\n";
  lines += "VALIDATED_BY=ESP32\n";
  lines += "VALIDATED_AT_MS=" + String(millis()) + "\n";

  SD.remove(path);
  File out = SD.open(path, FILE_WRITE);
  if (!out) return false;
  out.print(lines);
  out.close();

  return true;
}

bool moduleIsValidated(const String &filename) {
  if (!sdReady) return false;

  File f = SD.open(modulePath(filename), FILE_READ);
  if (!f) return false;

  bool ok = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line == "VALIDATED=YES") {
      ok = true;
      break;
    }
  }
  f.close();
  return ok;
}

// -------------------- MODULE COMMANDS --------------------
void listModules() {
  if (!sdReady) {
    btReply("ERROR|SD_NOT_READY");
    return;
  }

  File dir = SD.open("/MODULES");
  if (!dir || !dir.isDirectory()) {
    btReply("ERROR|MODULE_DIR");
    return;
  }

  btReply("MODULE_LIST_BEGIN");

  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      btReply(String("MODULE|") + String(file.name()));
    }
    file.close();
    file = dir.openNextFile();
  }

  dir.close();
  btReply("MODULE_LIST_END");
}

void readModule(const String &filename) {
  if (!sdReady) {
    btReply("ERROR|SD_NOT_READY");
    return;
  }

  File f = SD.open(modulePath(filename), FILE_READ);
  if (!f) {
    btReply("ERROR|MODULE_NOT_FOUND");
    return;
  }

  btReply("MODULE_BEGIN|" + normalizeModuleFilename(filename));
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length()) btReply("DATA|" + line);
  }
  f.close();
  btReply("MODULE_END");
}

void deleteModule(const String &filename) {
  if (!sdReady) {
    btReply("ERROR|SD_NOT_READY");
    return;
  }

  String path = modulePath(filename);

  if (!SD.exists(path)) {
    btReply("ERROR|MODULE_NOT_FOUND");
    return;
  }

  if (selectedModule.equalsIgnoreCase(normalizeModuleFilename(filename))) {
    selectedModule = "";
    pretestPassed = false;
  }

  if (SD.remove(path)) btReply("OK|MODULE_DELETED");
  else btReply("ERROR|MODULE_DELETE_FAILED");
}

void addOrUpdateModule(const String &command) {
  if (!sdReady) {
    btReply("ERROR|SD_NOT_READY");
    return;
  }

  // Format:
  // ADD_MODULE|filename|KEY=VALUE|KEY=VALUE|...
  int p1 = command.indexOf('|');
  if (p1 < 0) {
    btReply("ERROR|FORMAT");
    return;
  }

  int p2 = command.indexOf('|', p1 + 1);

  String filename;
  String content;

  if (p2 < 0) {
    filename = command.substring(p1 + 1);
    content = "";
  } else {
    filename = command.substring(p1 + 1, p2);
    content = command.substring(p2 + 1);
  }

  filename = normalizeModuleFilename(filename);

  if (!validFilename(filename)) {
    btReply("ERROR|INVALID_FILENAME");
    return;
  }

  // Replace | with newlines to create an INI-style file.
  content.replace('|', '\n');

  // Adding/updating always invalidates previous validation.
  content += "\nVALIDATED=NO\n";
  content += "VALIDATED_BY=\n";
  content += "VALIDATED_AT_MS=0\n";

  String path = modulePath(filename);
  if (SD.exists(path)) SD.remove(path);

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    btReply("ERROR|MODULE_WRITE_FAILED");
    return;
  }

  f.print(content);
  f.close();

  pretestPassed = false;

  btReply("OK|MODULE_SAVED|" + filename);
}

void selectModule(const String &filename) {
  String fn = normalizeModuleFilename(filename);

  if (!sdReady || !SD.exists(modulePath(fn))) {
    btReply("ERROR|MODULE_NOT_FOUND");
    return;
  }

  selectedModule = fn;
  pretestPassed = false;

  btReply("OK|MODULE_SELECTED|" + selectedModule);
}

// -------------------- PRETEST --------------------
bool runPretest(bool sendResult) {
  pretestPassed = false;

  if (faultActive) {
    if (sendResult) btReply("PRETEST=FAIL|FAULT=" + faultReason);
    return false;
  }

  if (selectedModule.length() == 0) {
    if (sendResult) btReply("PRETEST=FAIL|NO_MODULE_SELECTED");
    return false;
  }

  if (!moduleIsValidated(selectedModule)) {
    if (sendResult) btReply("PRETEST=FAIL|MODULE_NOT_VALIDATED");
    return false;
  }

  if (!sdReady) {
    if (sendResult) btReply("PRETEST=FAIL|SD_NOT_READY");
    return false;
  }

  updateNanoStatus();
  if (!nanoOnline) {
    if (sendResult) btReply("PRETEST=FAIL|NANO_OFFLINE");
    return false;
  }

  if (!inaReady) {
    if (sendResult) btReply("PRETEST=FAIL|INA219_NOT_READY");
    return false;
  }

  float v = readSupplyVoltage();
  if (v < SYSTEM_MIN_V || v > SYSTEM_MAX_V) {
    if (sendResult) btReply("PRETEST=FAIL|SUPPLY=" + String(v, 2));
    return false;
  }

  allOutputsOff();

  pretestPassed = true;

  if (sendResult) {
    btReply("PRETEST=PASS|READY_FOR_TEST|SUPPLY=" + String(v, 2) +
            "|MODULE=" + selectedModule);
  }

  return true;
}

// -------------------- LOGGING --------------------
void createTestLog() {
  if (!sdReady) return;

  activeLogFile = makeSessionName();

  File f = SD.open(activeLogFile, FILE_WRITE);
  if (!f) {
    activeLogFile = "";
    return;
  }

  f.println("TIME_MS,SUPPLY_V,DUT_V,CURRENT_A,POWER_W,POSITION_PCT,TEMP,DUT,NANO,FAULT,MODULE");
  f.close();
}

void logLiveData() {
  if (!testRunning || !sdReady || activeLogFile.length() == 0) return;

  unsigned long now = millis();
  if (now - lastLiveLog < LOG_INTERVAL_MS) return;
  lastLiveLog = now;

  float supply = readSupplyVoltage();
  float dutV = readDutVoltage();
  float current = readCurrentA();
  float power = readPowerW();
  float position = readPositionPercent();
  float temp = readTemperatureC();

  File f = SD.open(activeLogFile, FILE_APPEND);
  if (!f) return;

  f.print(now); f.print(",");
  f.print(supply, 3); f.print(",");
  f.print(dutV, 3); f.print(",");
  f.print(current, 3); f.print(",");
  f.print(power, 3); f.print(",");
  f.print(position, 2); f.print(",");
  f.print(temp, 3); f.print(",");
  f.print(digitalRead(DUT_RELAY) ? "ON" : "OFF"); f.print(",");
  f.print(nanoOnline ? "ONLINE" : "OFFLINE"); f.print(",");
  f.print(faultActive ? faultReason : "NO"); f.print(",");
  f.println(selectedModule);

  f.close();
}

void createReport() {
  if (!sdReady || activeLogFile.length() == 0) return;

  String base = activeLogFile.substring(activeLogFile.lastIndexOf('/') + 1);
  base.replace(".CSV", "");
  activeReportFile = "/REPORTS/" + base + ".TXT";

  File f = SD.open(activeReportFile, FILE_WRITE);
  if (!f) return;

  f.println("TIFF TESTER PRO V2 TEST REPORT");
  f.println("--------------------------------");
  f.println("Firmware=" FW_VERSION);
  f.println("Module=" + selectedModule);
  f.println("Log=" + activeLogFile);
  f.println("Duration_ms=" + String(millis() - testStartMillis));
  f.println("Final_Supply_V=" + String(readSupplyVoltage(), 2));
  f.println("Final_DUT_V=" + String(readDutVoltage(), 2));
  f.println("Final_Current_A=" + String(readCurrentA(), 2));
  f.println("Final_Power_W=" + String(readPowerW(), 2));
  f.println("Final_Position_pct=" + String(readPositionPercent(), 2));
  f.println("Final_Temperature=" + String(readTemperatureC(), 2));
  f.println("Fault=" + String(faultActive ? faultReason : "NONE"));
  f.close();
}

// -------------------- SAFETY --------------------
void triggerFault(const String &reason) {
  faultActive = true;
  faultReason = reason;

  allOutputsOff();
  testRunning = false;
  pretestPassed = false;

  printBoth("");
  printBoth("[SAFETY FAULT]");
  printBoth(reason);
  printBoth("[DUT] RELAY OFF");
  printBoth("[SAFETY] ALL OUTPUTS OFF");

  btReply("FAULT|" + reason);
}

void safetyCheck() {
  if (!testRunning || faultActive) return;

  float current = readCurrentA();
  if (current > SYSTEM_MAX_CURRENT) {
    if (overCurrentStart == 0) overCurrentStart = millis();

    if (millis() - overCurrentStart >= OVERCURRENT_MS) {
      triggerFault("DUT_OVERCURRENT");
      return;
    }
  } else {
    overCurrentStart = 0;
  }

  float supply = readSupplyVoltage();
  if (supply < SYSTEM_MIN_V || supply > SYSTEM_MAX_V) {
    triggerFault("SUPPLY_OUT_OF_RANGE");
    return;
  }

  updateNanoStatus();
  if (!nanoOnline) {
    triggerFault("NANO_OFFLINE");
    return;
  }
}

// -------------------- TEST CONTROL --------------------
void startTest() {
  if (testRunning) {
    btReply("ERROR|TEST_ALREADY_RUNNING");
    return;
  }

  if (!runPretest(false)) {
    btReply("START_TEST=BLOCKED");
    return;
  }

  createTestLog();

  if (activeLogFile.length() == 0) {
    btReply("START_TEST=BLOCKED|LOG_CREATE_FAILED");
    return;
  }

  digitalWrite(DUT_RELAY, HIGH);
  testRunning = true;
  pretestPassed = true;
  testStartMillis = millis();
  lastLiveLog = 0;
  overCurrentStart = 0;

  btReply("OK|TEST_STARTED|MODULE=" + selectedModule +
          "|LOG=" + activeLogFile);
}

void endTest() {
  digitalWrite(DUT_RELAY, LOW);
  testRunning = false;
  pretestPassed = false;

  if (activeLogFile.length()) createReport();

  btReply("OK|TEST_ENDED|LOG=" + activeLogFile +
          "|REPORT=" + activeReportFile);
}

void powerOn() {
  // Kept for app compatibility, but protected by full pretest.
  if (!runPretest(false)) {
    btReply("POWER_ON=BLOCKED");
    return;
  }

  digitalWrite(DUT_RELAY, HIGH);
  btReply("OK|DUT_POWER=ON");
}

void powerOff() {
  digitalWrite(DUT_RELAY, LOW);
  btReply("OK|DUT_POWER=OFF");
}

// -------------------- STATUS --------------------
String liveStatus() {
  float supply = readSupplyVoltage();
  float dutV = readDutVoltage();
  float current = readCurrentA();
  float power = readPowerW();
  float position = readPositionPercent();
  float temp = readTemperatureC();

  String s;
  s += "VOLTAGE=" + String(supply, 2);
  s += ",DUT_VOLTAGE=" + String(dutV, 2);
  s += ",CURRENT=" + String(current, 2);
  s += ",POWER=" + String(power, 2);
  s += ",POSITION=" + String(position, 1);
  s += ",TEMP=" + String(temp, 2);
  s += ",DUT=" + String(digitalRead(DUT_RELAY) ? "ON" : "OFF");
  s += ",FAULT=" + String(faultActive ? faultReason : "NO");
  s += ",NANO=" + String(nanoOnline ? "ONLINE" : "OFFLINE");
  s += ",INA219=" + String(inaReady ? "READY" : "ERROR");
  s += ",SD=" + String(sdReady ? "READY" : "ERROR");
  s += ",CAN=" + String(canReady ? "READY" : "ERROR");
  s += ",KLINE=" + String(klineReady ? "READY" : "ERROR");
  s += ",WATCHDOG=OK";
  s += ",MODULE=" + selectedModule;
  s += ",STATUS=" + String(testRunning ? "TESTING" : "IDLE");
  return s;
}

void printSerialLive() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("        TIFF TESTER PRO V2");
  Serial.println("========================================");
  Serial.println("Supply Voltage : " + String(readSupplyVoltage(), 2) + " V");
  Serial.println("DUT Voltage    : " + String(readDutVoltage(), 2) + " V");
  Serial.println("DUT Current    : " + String(readCurrentA(), 2) + " A");
  Serial.println("DUT Power      : " + String(readPowerW(), 2) + " W");
  Serial.println("Position       : " + String(readPositionPercent(), 1) + " %");
  Serial.println("Temperature    : " + String(readTemperatureC(), 2));
  Serial.println("----------------------------------------");
  Serial.println("DUT Relay      : " + String(digitalRead(DUT_RELAY) ? "ON" : "OFF"));
  Serial.println("Nano           : " + String(nanoOnline ? "ONLINE" : "OFFLINE"));
  Serial.println("INA219         : " + String(inaReady ? "READY" : "ERROR"));
  Serial.println("SD Card        : " + String(sdReady ? "READY" : "ERROR"));
  Serial.println("CAN            : " + String(canReady ? "READY" : "ERROR"));
  Serial.println("K-Line         : " + String(klineReady ? "READY" : "ERROR"));
  Serial.println("Watchdog       : OK");
  Serial.println("Module         : " + selectedModule);
  Serial.println("System Status  : " + String(testRunning ? "TESTING" : "IDLE"));
  Serial.println("========================================");
}

// -------------------- COMMANDS --------------------
void processCommand(String cmd) {
  cmd.trim();
  if (!cmd.length()) return;

  Serial.println("[CMD] " + cmd);

  String upper = cmd;
  upper.toUpperCase();

  if (upper == "STATUS" || upper == "READ_LIVE_DATA") {
    btReply(liveStatus());
    return;
  }

  if (upper == "LIST_MODULES") {
    listModules();
    return;
  }

  if (upper.startsWith("READ_MODULE|")) {
    readModule(cmd.substring(cmd.indexOf('|') + 1));
    return;
  }

  if (upper.startsWith("DELETE_MODULE|")) {
    deleteModule(cmd.substring(cmd.indexOf('|') + 1));
    return;
  }

  if (upper.startsWith("SELECT_MODULE|")) {
    selectModule(cmd.substring(cmd.indexOf('|') + 1));
    return;
  }

  if (upper.startsWith("ADD_MODULE|")) {
    addOrUpdateModule(cmd);
    return;
  }

  if (upper.startsWith("VALIDATE_MODULE|")) {
    String fn = cmd.substring(cmd.indexOf('|') + 1);
    String err, summary;

    if (!validateModuleFile(fn, err, summary)) {
      btReply("VALIDATION=FAIL|ERROR=" + err);
      return;
    }

    if (!markModuleValidated(fn)) {
      btReply("VALIDATION=FAIL|ERROR=VALIDATION_WRITE_FAILED");
      return;
    }

    btReply("VALIDATION=PASS|MODULE=" + normalizeModuleFilename(fn) + "|" + summary);
    return;
  }

  if (upper == "VALIDATE_ALL_MODULES") {
    if (!sdReady) {
      btReply("ERROR|SD_NOT_READY");
      return;
    }

    File dir = SD.open("/MODULES");
    if (!dir || !dir.isDirectory()) {
      btReply("ERROR|MODULE_DIR");
      return;
    }

    btReply("VALIDATION_ALL_BEGIN");

    File file = dir.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String name = String(file.name());
        file.close();

        String err, summary;
        if (validateModuleFile(name, err, summary)) {
          if (markModuleValidated(name))
            btReply("PASS|" + name + "|" + summary);
          else
            btReply("FAIL|" + name + "|VALIDATION_WRITE_FAILED");
        } else {
          btReply("FAIL|" + name + "|" + err);
        }
      } else {
        file.close();
      }

      file = dir.openNextFile();
    }

    dir.close();
    btReply("VALIDATION_ALL_END");
    return;
  }

  if (upper == "PRETEST" || upper == "VALIDATE_TEST_READY") {
    runPretest(true);
    return;
  }

  if (upper == "START_TEST") {
    startTest();
    return;
  }

  if (upper == "END_TEST") {
    endTest();
    return;
  }

  if (upper == "POWER_ON") {
    powerOn();
    return;
  }

  if (upper == "POWER_OFF") {
    powerOff();
    return;
  }

  if (upper == "ALL_OFF") {
    allOutputsOff();
    testRunning = false;
    pretestPassed = false;
    btReply("OK|ALL_OUTPUTS_OFF");
    return;
  }

  if (upper == "RESET_FAULT") {
    faultActive = false;
    faultReason = "";
    overCurrentStart = 0;
    allOutputsOff();
    btReply("OK|FAULT_RESET");
    return;
  }

  if (upper == "RELAY1_ON") { nanoOutputCommand(NANO_R1_ON, "RELAY1=ON"); return; }
  if (upper == "RELAY1_OFF") { nanoOutputCommand(NANO_R1_OFF, "RELAY1=OFF"); return; }
  if (upper == "RELAY2_ON") { nanoOutputCommand(NANO_R2_ON, "RELAY2=ON"); return; }
  if (upper == "RELAY2_OFF") { nanoOutputCommand(NANO_R2_OFF, "RELAY2=OFF"); return; }
  if (upper == "RELAY3_ON") { nanoOutputCommand(NANO_R3_ON, "RELAY3=ON"); return; }
  if (upper == "RELAY3_OFF") { nanoOutputCommand(NANO_R3_OFF, "RELAY3=OFF"); return; }
  if (upper == "RELAY4_ON") { nanoOutputCommand(NANO_R4_ON, "RELAY4=ON"); return; }
  if (upper == "RELAY4_OFF") { nanoOutputCommand(NANO_R4_OFF, "RELAY4=OFF"); return; }

  if (upper == "MOSFET1_ON") { nanoOutputCommand(NANO_M1_ON, "MOSFET1=ON"); return; }
  if (upper == "MOSFET1_OFF") { nanoOutputCommand(NANO_M1_OFF, "MOSFET1=OFF"); return; }
  if (upper == "MOSFET2_ON") { nanoOutputCommand(NANO_M2_ON, "MOSFET2=ON"); return; }
  if (upper == "MOSFET2_OFF") { nanoOutputCommand(NANO_M2_OFF, "MOSFET2=OFF"); return; }

  if (upper == "SERIAL_LIVE") {
    printSerialLive();
    btReply("OK|SERIAL_LIVE");
    return;
  }

  btReply("ERROR|UNKNOWN_COMMAND");
}

// -------------------- INPUTS --------------------
String serialBuffer;
String btBuffer;

void readSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length()) {
        processCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
      if (serialBuffer.length() > 700) serialBuffer = "";
    }
  }
}

void readBluetoothCommands() {
  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n' || c == '\r') {
      if (btBuffer.length()) {
        processCommand(btBuffer);
        btBuffer = "";
      }
    } else {
      btBuffer += c;
      if (btBuffer.length() > 700) btBuffer = "";
    }
  }
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(DUT_RELAY, OUTPUT);
  digitalWrite(DUT_RELAY, LOW);

  analogReadResolution(12);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  Serial.println();
  Serial.println("========================================");
  Serial.println("TIFF TESTER PRO V2.2");
  Serial.println("Bluetooth-only firmware");
  Serial.println("========================================");

  if (SerialBT.begin(BT_NAME)) {
    Serial.println("Bluetooth: READY");
    Serial.println("BT Name: " BT_NAME);
  } else {
    Serial.println("Bluetooth: ERROR");
  }

  if (ina219.begin()) {
    inaReady = true;
    Serial.println("INA219: READY");
  } else {
    inaReady = false;
    Serial.println("INA219: ERROR");
  }

  if (SD.begin(SD_CS, SPI)) {
    sdReady = true;
    ensureDirectories();
    Serial.println("SD Card: READY");
  } else {
    sdReady = false;
    Serial.println("SD Card: ERROR / NOT INSTALLED");
  }

  canReady = initCANHardware();
  Serial.println("CAN: " + String(canReady ? "READY" : "ERROR"));

  Serial2.begin(10400, SERIAL_8N1, KLINE_RX, KLINE_TX);
  klineReady = true;
  Serial.println("K-Line UART: READY");

  allOutputsOff();

  // ESP32 task watchdog - 5 seconds.
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 5000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  delay(100);
  updateNanoStatus();

  Serial.println("Nano: " + String(nanoOnline ? "ONLINE" : "OFFLINE"));
  Serial.println("System ready.");
}

// -------------------- LOOP --------------------
void loop() {
  esp_task_wdt_reset();

  readSerialCommands();
  readBluetoothCommands();

  unsigned long now = millis();

  if (now - lastHeartbeat >= NANO_HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    sendNanoHeartbeat();
    updateNanoStatus();
  }

  safetyCheck();
  logLiveData();

  static unsigned long lastSerial = 0;
  if (now - lastSerial >= 2000) {
    lastSerial = now;
    printSerialLive();
  }

  delay(5);
}

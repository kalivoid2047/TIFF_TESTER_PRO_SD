/*
  TIFF_TESTER_PRO_SD - ESP32 Main Controller
  Bench-first automotive module tester.

  Hardware:
    ESP32 DevKit
    SD card (SPI)
    MCP2515 CAN module
    L9637D K-Line interface
    UART link to Arduino Nano safety controller

  IMPORTANT:
    This is a framework/base controller. OEM diagnostic/service procedures
    must be validated per module before use. Do not send arbitrary UDS
    routines to a vehicle/module.

  BOARD SETTING REQUIRED:
    Wi-Fi + WebServer + SD + BLE + CAN + K-Line together exceed the ESP32's
    default partition scheme's app space. In the Arduino IDE, set
    Tools > Partition Scheme to "Huge APP (3MB No OTA/1MB SPIFFS)" (or
    equivalent) before compiling/uploading, or this will fail to build
    with "text section exceeds available space in board". CI builds this
    with fqbn esp32:esp32:esp32:PartitionScheme=huge_app — see
    .github/workflows/ci.yml.

  This sketch is organized as several tab files, all part of the same
  build (see shared_types.h for why struct types live in a real header
  rather than in a .ino tab):
    TIFF_TESTER_PRO_SD_ESP32.ino  - setup/loop, safety UART, web dashboard
    config.ino                    - SD-backed AP password / BLE PIN config
    can_mcp2515.ino                - MCP2515 CAN driver
    kline_iso14230.ino             - ISO 14230 K-Line driver
    ble_service.ino                - BLE GATT service for a future mobile app
    test_orchestrator.ino          - module-profile-driven test execution + reports
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "shared_types.h"

// ---------- Pins ----------
#define SD_CS           5
#define CAN_CS         15
#define CAN_INT         4

#define NANO_RX        16
#define NANO_TX        17

#define KLINE_RX       25
#define KLINE_TX       26

#define DUT_VOLT_PIN   34
#define DUT_CURR_PIN   35
#define SUPPLY_VOLT_PIN 32

#define STATUS_LED      2

HardwareSerial NanoSerial(2);
HardwareSerial KlineSerial(1);
WebServer server(80);

// ---------- BLE ----------
// Vendor UUIDs minted for this project. If you ship another product with
// its own BLE service on the same base, generate fresh UUIDs rather than
// reusing these.
#define BLE_SERVICE_UUID       "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_AUTH_UUID     "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_STATUS_UUID   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_COMMAND_UUID  "6e400004-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_RESULT_UUID   "6e400005-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_MODULES_UUID  "6e400006-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_DEVINFO_UUID  "6e400007-b5a3-f393-e0a9-e50e24dcca9e"

BLEServer *bleServer = nullptr;
BLECharacteristic *bleAuthChar = nullptr;
BLECharacteristic *bleStatusChar = nullptr;
BLECharacteristic *bleCommandChar = nullptr;
BLECharacteristic *bleResultChar = nullptr;
BLECharacteristic *bleModulesChar = nullptr;
BLECharacteristic *bleDevInfoChar = nullptr;
bool bleClientConnected = false;
bool bleAuthenticated = false;
uint8_t blePinFailCount = 0;
uint32_t blePinLockoutUntil = 0;

// ---------- System config (SD-backed, see config.ino) ----------
SystemConfig sysConfig;

// ---------- Active module profile (see test_orchestrator.ino) ----------
ModuleProfile activeModule;

// [SERVICE] routines are read and reported (see test_orchestrator.ino) but
// never executed by this firmware build — see Documentation/SRS.md and the
// header comment on runServiceRoutine().
#define SERVICE_ROUTINES_ENABLED false

// ---------- Safety ----------
const uint32_t HEARTBEAT_MS = 250;
const uint32_t STATUS_POLL_MS = 500;
uint32_t lastHeartbeat = 0;
uint32_t lastStatusPoll = 0;

NanoStatus nano;

String serialLine;

void sendNano(const String &cmd) {
  NanoSerial.println(cmd);
}

void parseNanoLine(String s) {
  s.trim();
  if (!s.length()) return;

  // STATUS,relay,fault,estop,watchdog,supply,dut,current,faulttext
  if (s.startsWith("STATUS,")) {
    String a[9];
    int start = 0, n = 0;
    for (int i = 0; i <= (int)s.length() && n < 9; i++) {
      if (i == (int)s.length() || s[i] == ',') {
        a[n++] = s.substring(start, i);
        start = i + 1;
      }
    }
    if (n >= 8) {
      nano.relay = a[1].toInt();
      nano.fault = a[2].toInt();
      nano.estop = a[3].toInt();
      nano.watchdog = a[4].toInt();
      nano.supplyV = a[5].toFloat();
      nano.dutV = a[6].toFloat();
      nano.currentA = a[7].toFloat();
      if (n >= 9) nano.faultText = a[8];
    }
    return;
  }

  // CONFIG,supplyDiv,dutDiv,curZero,curScale,relayActiveLow,calibrated
  // (currently just logged; exposed via GET /api/nano/config on demand)
  if (s.startsWith("CONFIG,")) {
    logLine("/LOGS/nano_config.log", s);
    return;
  }
}

void readNano() {
  while (NanoSerial.available()) {
    char c = NanoSerial.read();
    if (c == '\n') {
      parseNanoLine(serialLine);
      serialLine = "";
    } else if (c != '\r' && serialLine.length() < 200) {
      serialLine += c;
    }
  }
}

float readVoltage(uint8_t pin, float dividerRatio) {
  uint16_t raw = analogRead(pin);
  return (raw / 4095.0f) * 3.3f * dividerRatio;
}

String htmlEscape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  return s;
}

bool validModuleText(const String &data, String &reason) {
  if (data.length() < 20) { reason = "Module file is too short."; return false; }
  if (data.indexOf("[MODULE]") < 0) { reason = "Missing [MODULE] section."; return false; }
  if (data.indexOf("id=") < 0) { reason = "Missing module id."; return false; }
  if (data.indexOf("max_current_a=") < 0) { reason = "Missing max_current_a."; return false; }
  if (data.indexOf("min_voltage_v=") < 0) { reason = "Missing min_voltage_v."; return false; }
  if (data.indexOf("max_voltage_v=") < 0) { reason = "Missing max_voltage_v."; return false; }
  return true;
}

String modulePathFromId(String id) {
  id.trim();
  id.replace("..", "");
  id.replace("/", "_");
  id.replace("\\", "_");
  return "/MODULES/" + id + ".INI";
}

void handleRoot() {
  String page = R"HTML(
<!doctype html><html><head><meta name='viewport' content='width=device-width'>
<title>TIFF TESTER PRO SD</title>
<style>body{font-family:Arial;margin:20px;max-width:900px}button{padding:12px;margin:4px}
textarea{width:100%;height:300px} .card{border:1px solid #aaa;padding:15px;margin:10px 0}</style>
</head><body><h1>TIFF TESTER PRO SD</h1>
<div class='card'><b>Bench Safety</b><p id='status'>Loading...</p>
<button onclick="fetch('/api/power/off')">DUT OFF</button>
<button onclick="fetch('/api/status').then(()=>location.reload())">Refresh</button></div>
<div class='card'><h2>Add / Update Module</h2>
<form method='POST' action='/api/module'><textarea name='data'
placeholder='Paste validated .INI module profile here'></textarea><br>
<button type='submit'>Validate & Save</button></form></div>
<div class='card'><h2>SD Modules</h2><a href='/api/modules'>View module list</a></div>
<div class='card'><h2>Reports</h2><a href='/api/reports'>View saved reports</a></div>
<script>fetch('/api/status').then(r=>r.text()).then(t=>document.getElementById('status').innerText=t)</script>
</body></html>
)HTML";
  server.send(200, "text/html", page);
}

void handleStatus() {
  readNano();
  String out = "Relay=" + String(nano.relay) +
               " Fault=" + String(nano.fault) +
               " EStop=" + String(nano.estop) +
               " Watchdog=" + String(nano.watchdog) +
               " Supply=" + String(nano.supplyV,2) + "V" +
               " DUT=" + String(nano.dutV,2) + "V" +
               " Current=" + String(nano.currentA,2) + "A";
  if (nano.faultText.length()) out += " FaultText=" + nano.faultText;
  server.send(200, "text/plain", out);
}

void handlePowerOn() {
  sendNano("POWER_ON");
  server.send(200, "text/plain", "POWER_ON requested; Nano will perform safety checks.");
}

void handlePowerOff() {
  sendNano("POWER_OFF");
  server.send(200, "text/plain", "POWER_OFF requested.");
}

void handleModuleSave() {
  String data = server.arg("data");
  String reason;
  if (!validModuleText(data, reason)) {
    server.send(400, "text/plain", "REJECTED: " + reason);
    return;
  }

  int idPos = data.indexOf("id=");
  int end = data.indexOf('\n', idPos);
  String id = data.substring(idPos + 3, end < 0 ? data.length() : end);
  id.trim();

  String path = modulePathFromId(id);
  if (path == "/MODULES/.INI" || path == "/MODULES/SYSTEM.INI") {
    server.send(403, "text/plain", "Protected filename.");
    return;
  }

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    server.send(500, "text/plain", "Could not open SD file.");
    return;
  }
  f.print(data);
  f.close();

  server.send(200, "text/plain", "Saved: " + path);
}

// Shared by the web API and the BLE modules characteristic (ble_service.ino)
// so both surfaces list the same "MODULES\n<file1>\n<file2>..." text.
// Returns false (with `out` untouched) if the directory can't be opened.
bool moduleListText(String &out) {
  File dir = SD.open("/MODULES");
  if (!dir || !dir.isDirectory()) return false;

  out = "MODULES\n";
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) out += String(f.name()) + "\n";
    f.close();
    f = dir.openNextFile();
  }
  return true;
}

void handleModules() {
  String out;
  if (!moduleListText(out)) {
    server.send(500, "text/plain", "MODULES directory unavailable.");
    return;
  }
  server.send(200, "text/plain", out);
}

void setupWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/power/on", HTTP_GET, handlePowerOn);
  server.on("/api/power/off", HTTP_GET, handlePowerOff);
  server.on("/api/module", HTTP_POST, handleModuleSave);
  server.on("/api/modules", HTTP_GET, handleModules);

  // Module-profile-driven test orchestration (test_orchestrator.ino)
  server.on("/api/module/select", HTTP_POST, handleModuleSelect);
  server.on("/api/module/active", HTTP_GET, handleModuleActive);
  server.on("/api/test/run", HTTP_POST, handleTestRun);
  server.on("/api/reports", HTTP_GET, handleReportsList);

  // System config: AP password / BLE PIN (config.ino)
  server.on("/api/config", HTTP_GET, handleConfigStatus);
  server.on("/api/config/wifi", HTTP_POST, handleConfigWifi);
  server.on("/api/config/pin", HTTP_POST, handleConfigPin);

  server.begin();
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  analogReadResolution(12);

  NanoSerial.begin(115200, SERIAL_8N1, NANO_RX, NANO_TX);

  bool sdReady = SD.begin(SD_CS);
  if (!sdReady) {
    Serial.println("SD init failed.");
  } else {
    SD.mkdir("/MODULES");
    SD.mkdir("/REPORTS");
    SD.mkdir("/LOGS");
    Serial.println("SD ready.");
    loadSystemConfig(); // config.ino - falls back to defaults if SD unavailable
  }

  canInit(500000, 8000000);  // can_mcp2515.ino - 500 kbps @ 8 MHz osc, verify for your board
  klineInit();                // kline_iso14230.ino

  WiFi.mode(WIFI_AP);
  WiFi.softAP("TIFF_TESTER", sysConfig.apPassword.c_str());
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  setupWeb();
  bleInit(); // ble_service.ino

  if (sdReady) logLine("/LOGS/system.log", "Boot complete.");
  lastHeartbeat = millis();
}

void loop() {
  server.handleClient();
  readNano();

  uint32_t now = millis();

  if (now - lastHeartbeat >= HEARTBEAT_MS) {
    sendNano("HEARTBEAT");
    lastHeartbeat = now;
  }

  if (now - lastStatusPoll >= STATUS_POLL_MS) {
    sendNano("STATUS");
    bleNotifyStatus(); // ble_service.ino
    lastStatusPoll = now;
  }

  digitalWrite(STATUS_LED, (now / 500) % 2);
}

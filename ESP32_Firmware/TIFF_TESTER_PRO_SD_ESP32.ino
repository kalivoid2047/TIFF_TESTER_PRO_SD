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
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>

// ---------- Pins ----------
#define SD_CS           5
#define CAN_CS         15
#define CAN_INT         4

#define NANO_RX        16
#define NANO_TX        17

#define DUT_VOLT_PIN   34
#define DUT_CURR_PIN   35
#define SUPPLY_VOLT_PIN 32

#define STATUS_LED      2

HardwareSerial NanoSerial(2);
WebServer server(80);

// ---------- Safety ----------
const uint32_t HEARTBEAT_MS = 250;
const uint32_t STATUS_POLL_MS = 500;
uint32_t lastHeartbeat = 0;
uint32_t lastStatusPoll = 0;

struct NanoStatus {
  bool relay = false;
  bool fault = false;
  bool estop = false;
  bool watchdog = false;
  float supplyV = 0;
  float dutV = 0;
  float currentA = 0;
  String faultText = "";
} nano;

String serialLine;

void sendNano(const String &cmd) {
  NanoSerial.println(cmd);
}

void parseNanoLine(String s) {
  s.trim();
  if (!s.length()) return;

  // STATUS,relay,fault,estop,watchdog,supply,dut,current,faulttext
  if (s.startsWith("STATUS,")) {
    int p[9];
    int idx = 0;
    for (int i=0; i<9; i++) p[i] = -1;
    for (int i=0; i<(int)s.length() && idx<9; i++) {
      if (s[i] == ',') p[++idx] = i;
    }
    // Simpler CSV extraction
    String a[9];
    int start = 0, n = 0;
    for (int i=0; i<=s.length() && n<9; i++) {
      if (i == s.length() || s[i] == ',') {
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

void handleModules() {
  File dir = SD.open("/MODULES");
  if (!dir || !dir.isDirectory()) {
    server.send(500, "text/plain", "MODULES directory unavailable.");
    return;
  }

  String out = "MODULES\n";
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) out += String(f.name()) + "\n";
    f.close();
    f = dir.openNextFile();
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
  server.begin();
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  analogReadResolution(12);

  NanoSerial.begin(115200, SERIAL_8N1, NANO_RX, NANO_TX);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed.");
  } else {
    SD.mkdir("/MODULES");
    SD.mkdir("/REPORTS");
    SD.mkdir("/LOGS");
    Serial.println("SD ready.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP("TIFF_TESTER", "tifftester");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  setupWeb();
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
    lastStatusPoll = now;
  }

  digitalWrite(STATUS_LED, (now / 500) % 2);
}

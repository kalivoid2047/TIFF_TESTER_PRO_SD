// System configuration, stored on SD as /CONFIG.INI.
//
// Holds the Wi-Fi AP password and BLE pairing PIN so they are no longer
// hard-coded in firmware (see Documentation/PROJECT_STATUS.md - "Wi-Fi AP
// password is a hardcoded default"). They still default to the same
// values as before on first boot (so existing setups keep working), but
// are now editable via the web API below and persist across reflashes.
//
// File format (simple key=value, one per line):
//   [SYSTEM]
//   ap_password=...
//   ble_pin=...

#define CONFIG_PATH "/CONFIG.INI"

static String configLine(const String &data, const String &key) {
  int pos = data.indexOf(key + "=");
  if (pos < 0) return "";
  int end = data.indexOf('\n', pos);
  String v = data.substring(pos + key.length() + 1, end < 0 ? data.length() : end);
  v.trim();
  return v;
}

// Shared, simple append-only logger used by every subsystem below.
void logLine(const char *path, const String &line) {
  File f = SD.open(path, FILE_APPEND);
  if (!f) return;
  f.print(millis());
  f.print(" ");
  f.println(line);
  f.close();
}

void saveSystemConfig() {
  File f = SD.open(CONFIG_PATH, FILE_WRITE);
  if (!f) return;
  f.println("[SYSTEM]");
  f.print("ap_password=");
  f.println(sysConfig.apPassword);
  f.print("ble_pin=");
  f.println(sysConfig.blePin);
  f.close();
}

void loadSystemConfig() {
  if (!SD.exists(CONFIG_PATH)) {
    // First boot: persist the documented defaults so they're visible and
    // editable, and make it obvious in the logs that defaults are active.
    saveSystemConfig();
    logLine("/LOGS/system.log",
             "WARNING: /CONFIG.INI created with default ap_password/ble_pin. Change them before field use.");
    sysConfig.loadedFromSD = false;
    return;
  }

  File f = SD.open(CONFIG_PATH, FILE_READ);
  if (!f) return;
  String data;
  while (f.available()) data += (char)f.read();
  f.close();

  String pw = configLine(data, "ap_password");
  String pin = configLine(data, "ble_pin");
  if (pw.length()) sysConfig.apPassword = pw;
  if (pin.length()) sysConfig.blePin = pin;
  sysConfig.loadedFromSD = true;

  bool stillDefault = (sysConfig.apPassword == "tifftester" || sysConfig.blePin == "TIFF2026");
  if (stillDefault) {
    logLine("/LOGS/system.log", "WARNING: default ap_password and/or ble_pin still active.");
  }
}

void handleConfigStatus() {
  String out = "{";
  out += "\"ap_password_is_default\":" + String(sysConfig.apPassword == "tifftester" ? "true" : "false") + ",";
  out += "\"ble_pin_is_default\":" + String(sysConfig.blePin == "TIFF2026" ? "true" : "false") + ",";
  out += "\"config_loaded_from_sd\":" + String(sysConfig.loadedFromSD ? "true" : "false");
  out += "}";
  server.send(200, "application/json", out);
}

void handleConfigWifi() {
  String cur = server.arg("current_password");
  String next = server.arg("new_password");
  if (cur != sysConfig.apPassword) {
    server.send(403, "text/plain", "Current password incorrect.");
    return;
  }
  if (next.length() < 8) {
    server.send(400, "text/plain", "New password must be at least 8 characters (Wi-Fi AP minimum).");
    return;
  }
  sysConfig.apPassword = next;
  saveSystemConfig();
  logLine("/LOGS/system.log", "AP password changed via web API.");
  server.send(200, "text/plain", "AP password updated. Reboot to apply it to the active AP.");
}

void handleConfigPin() {
  String cur = server.arg("current_pin");
  String next = server.arg("new_pin");
  if (cur != sysConfig.blePin) {
    server.send(403, "text/plain", "Current PIN incorrect.");
    return;
  }
  if (next.length() < 6) {
    server.send(400, "text/plain", "New PIN must be at least 6 characters.");
    return;
  }
  sysConfig.blePin = next;
  saveSystemConfig();
  logLine("/LOGS/system.log", "BLE PIN changed via web API.");
  server.send(200, "text/plain", "BLE PIN updated.");
}

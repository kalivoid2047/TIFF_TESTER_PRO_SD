// BLE peripheral exposing status/command/result characteristics for a
// future companion mobile app, per Documentation/API_PROTOCOL_SPEC.md.
// This is firmware-side plumbing only — no mobile app exists in this repo
// yet (see Documentation/ROADMAP.md Phase 3).
//
// Auth model: "Option A" from that spec — an app-layer PIN check on the
// Auth characteristic gates writes to the Command characteristic. This has
// NOT been hardened with BLE link-layer encryption/bonding (see
// API_PROTOCOL_SPEC.md §3) — treat this as a bench bring-up of the
// protocol, not a field-ready secure link. Uses the ESP32 Arduino core's
// bundled BLE library, so no extra Library Manager install is required.

class BleServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *s) override {
    bleClientConnected = true;
    bleAuthenticated = false;
  }
  void onDisconnect(BLEServer *s) override {
    bleClientConnected = false;
    bleAuthenticated = false;
    BLEDevice::startAdvertising();
  }
};

class BleAuthCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    if (millis() < blePinLockoutUntil) {
      bleAuthenticated = false;
      return;
    }
    String pin = c->getValue().c_str();
    if (pin == sysConfig.blePin) {
      bleAuthenticated = true;
      blePinFailCount = 0;
    } else {
      bleAuthenticated = false;
      blePinFailCount++;
      if (blePinFailCount >= 5) {
        // Growing backoff after repeated failures, per
        // Documentation/API_PROTOCOL_SPEC.md §3.
        blePinLockoutUntil = millis() + 30000UL * (blePinFailCount - 4);
      }
    }
  }
};

// Defined in TIFF_TESTER_PRO_SD_ESP32.ino, shared with the /api/modules web
// handler so both surfaces list the same SD-backed module files.
bool moduleListText(String &out);

class BleModulesCallbacks : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *c) override {
    String out;
    if (moduleListText(out)) c->setValue(out.c_str());
  }
};

class BleCommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    String cmd = c->getValue().c_str();
    cmd.trim();

    // POWER_OFF/STOP_TEST are always allowed (safety-favorable), matching
    // Documentation/API_PROTOCOL_SPEC.md §1.3 — everything else requires
    // PIN auth first.
    bool alwaysAllowed = (cmd == "POWER_OFF" || cmd == "STOP_TEST" || cmd == "DIAG_STOP");
    if (!bleAuthenticated && !alwaysAllowed) return;

    // RELAY:<n>,<0|1> (1=DUT, 2-4=auxiliary) and RELAY_TEST:<n>. The Nano
    // enforces e-stop/fault/heartbeat gating; nothing is decided here.
    if (cmd.startsWith("RELAY:")) {
      String p = cmd.substring(6);
      int c = p.indexOf(',');
      int n = p.substring(0, c < 0 ? 0 : c).toInt();
      int v = c < 0 ? -1 : p.substring(c + 1).toInt();
      if (n >= 1 && n <= 4 && (v == 0 || v == 1))
        sendNano("RELAY," + String(n) + "," + String(v));
      return;
    }
    if (cmd.startsWith("RELAY_TEST:")) {
      int n = cmd.substring(11).toInt();
      if (n >= 1 && n <= 4) sendNano("RELAY_TEST," + String(n));
      return;
    }

    // SET_POLARITY:<0|1> - relay module polarity for all four relays
    // (1 = active-low). The Nano forces every relay off before and after
    // changing it and persists the setting to EEPROM.
    if (cmd.startsWith("SET_POLARITY:")) {
      int v = cmd.substring(13).toInt();
      sendNano("SET_RELAY_POLARITY," + String(v == 1 ? 1 : 0));
      logLine("/LOGS/system.log", "Relay polarity set via BLE: " + String(v == 1 ? "active-low" : "active-high"));
      return;
    }

    // CAN / UDS / K-Line / KWP diagnostics (diag_engine.ino). These are only
    // queued here; they cannot switch relays (see Documentation/DIAGNOSTICS.md).
    if (diagHandleCommand(cmd)) return;

    if (cmd == "POWER_ON") { sendNano("POWER_ON"); return; }
    if (cmd == "POWER_OFF") { sendNano("POWER_OFF"); return; }
    if (cmd == "RESET_FAULT") { sendNano("RESET_FAULT"); return; }

    if (cmd.startsWith("SELECT_MODULE:")) {
      loadActiveModule(cmd.substring(String("SELECT_MODULE:").length()));
      return;
    }

    // SET_PIN:<new_pin> — already authenticated with the *current* PIN to
    // reach this point, so no separate current-PIN check is needed here
    // (unlike the web API's /api/config/pin, which isn't gated by a prior
    // auth step and so checks it explicitly). Mirrors
    // Documentation/SRS.md FR-CONN-4 / NFR-SEC-2.
    if (cmd.startsWith("SET_PIN:")) {
      String next = cmd.substring(String("SET_PIN:").length());
      next.trim();
      if (next.length() >= 6) {
        sysConfig.blePin = next;
        saveSystemConfig();
        logLine("/LOGS/system.log", "BLE PIN changed via BLE SET_PIN command.");
      }
      return;
    }

    if (cmd.startsWith("RUN_TEST:")) {
      String test = cmd.substring(String("RUN_TEST:").length());
      TestResult r;
      if (test == "resistance") r = runResistanceTest();
      else if (test == "short_to_ground") r = runShortToGroundTest();
      else if (test == "current_monitor") r = runCurrentMonitorTest();
      else return;

      notifyResult(r);
      return;
    }

    // RUN_INJECTOR_TEST:<channel>,<pulse_width_ms>,<duration_s>
    // RUN_COIL_TEST:<channel>,<dwell_ms>,<duration_s>
    // RUN_ALL_INJECTORS:<pulse_width_ms>,<duration_per_s>
    //
    // These match the mobile app's Injector/Coil/All-Injectors test
    // screens (Documentation/UI_UX_SPEC.md §4.6-4.8), but there is no
    // injector/coil driver hardware on this board yet (Documentation/
    // ROADMAP.md Phase 1) — so, honestly, they report NOT_IMPLEMENTED
    // instead of either faking a pass or leaving the app waiting forever
    // for a result that will never arrive.
    if (cmd.startsWith("RUN_INJECTOR_TEST:")) {
      String params = cmd.substring(String("RUN_INJECTOR_TEST:").length());
      TestResult r = runUnavailableHardwareTest("injector");
      r.response = "channel_params=" + params + " - " + r.response;
      notifyResult(r);
      return;
    }

    if (cmd.startsWith("RUN_COIL_TEST:")) {
      String params = cmd.substring(String("RUN_COIL_TEST:").length());
      TestResult r = runUnavailableHardwareTest("coil");
      r.response = "channel_params=" + params + " - " + r.response;
      notifyResult(r);
      return;
    }

    if (cmd.startsWith("RUN_ALL_INJECTORS:")) {
      String params = cmd.substring(String("RUN_ALL_INJECTORS:").length());
      TestResult r = runUnavailableHardwareTest("all_injectors");
      r.response = "params=" + params + " - " + r.response;
      notifyResult(r);
      return;
    }

    // STOP_TEST: every test above runs and completes synchronously (single
    // sensor read, or an immediate NOT_IMPLEMENTED) rather than a
    // long-running pulse train. The one in-flight thing it can interrupt is
    // a relay test (and the Nano's supervised test window), so forward it.
    if (cmd == "STOP_TEST") {
      sendNano("RELAY_TEST_STOP");
      sendNano("TEST_STOP");
      return;
    }
  }
};

void notifyResult(const TestResult &r) {
  appendResultToReport(r);
  if (!bleResultChar) return;
  String out = r.testType + "," + (r.pass ? "PASS" : "FAIL") + "," + r.response;
  bleResultChar->setValue(out.c_str());
  bleResultChar->notify();
}

void bleInit() {
  BLEDevice::init("TiffTester");
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new BleServerCallbacks());

  BLEService *svc = bleServer->createService(BLE_SERVICE_UUID);

  bleAuthChar = svc->createCharacteristic(BLE_CHAR_AUTH_UUID, BLECharacteristic::PROPERTY_WRITE);
  bleAuthChar->setCallbacks(new BleAuthCallbacks());

  bleStatusChar = svc->createCharacteristic(
      BLE_CHAR_STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  bleStatusChar->addDescriptor(new BLE2902());

  bleCommandChar = svc->createCharacteristic(BLE_CHAR_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
  bleCommandChar->setCallbacks(new BleCommandCallbacks());

  bleResultChar = svc->createCharacteristic(
      BLE_CHAR_RESULT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  bleResultChar->addDescriptor(new BLE2902());

  bleModulesChar = svc->createCharacteristic(BLE_CHAR_MODULES_UUID, BLECharacteristic::PROPERTY_READ);
  bleModulesChar->setCallbacks(new BleModulesCallbacks());
  {
    String out;
    if (moduleListText(out)) bleModulesChar->setValue(out.c_str());
  }

  bleDevInfoChar = svc->createCharacteristic(BLE_CHAR_DEVINFO_UUID, BLECharacteristic::PROPERTY_READ);
  bleDevInfoChar->setValue("TIFF_TESTER_PRO_SD_ESP32");

  svc->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("BLE: advertising as TiffTester.");
}

void bleNotifyStatus() {
  if (!bleClientConnected || !bleStatusChar) return;

  String out = liveStatusJson(); // live_data.ino

  bleStatusChar->setValue(out.c_str());
  bleStatusChar->notify();
}

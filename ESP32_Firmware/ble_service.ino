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

class BleCommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    String cmd = c->getValue().c_str();
    cmd.trim();

    // POWER_OFF/STOP_TEST are always allowed (safety-favorable), matching
    // Documentation/API_PROTOCOL_SPEC.md §1.3 — everything else requires
    // PIN auth first.
    bool alwaysAllowed = (cmd == "POWER_OFF" || cmd == "STOP_TEST");
    if (!bleAuthenticated && !alwaysAllowed) return;

    if (cmd == "POWER_ON") { sendNano("POWER_ON"); return; }
    if (cmd == "POWER_OFF") { sendNano("POWER_OFF"); return; }
    if (cmd == "RESET_FAULT") { sendNano("RESET_FAULT"); return; }

    if (cmd.startsWith("SELECT_MODULE:")) {
      loadActiveModule(cmd.substring(String("SELECT_MODULE:").length()));
      return;
    }

    if (cmd.startsWith("RUN_TEST:")) {
      String test = cmd.substring(String("RUN_TEST:").length());
      TestResult r;
      if (test == "resistance") r = runResistanceTest();
      else if (test == "short_to_ground") r = runShortToGroundTest();
      else if (test == "current_monitor") r = runCurrentMonitorTest();
      else return;

      appendResultToReport(r);

      if (bleResultChar) {
        String out = r.testType + "," + (r.pass ? "PASS" : "FAIL") + "," + r.response;
        bleResultChar->setValue(out.c_str());
        bleResultChar->notify();
      }
      return;
    }
  }
};

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

  String out = "{";
  out += "\"relay\":" + String(nano.relay) + ",";
  out += "\"fault\":" + String(nano.fault) + ",";
  out += "\"estop\":" + String(nano.estop) + ",";
  out += "\"watchdog\":" + String(nano.watchdog) + ",";
  out += "\"supply_v\":" + String(nano.supplyV, 2) + ",";
  out += "\"dut_v\":" + String(nano.dutV, 2) + ",";
  out += "\"current_a\":" + String(nano.currentA, 2) + ",";
  out += "\"fault_text\":\"" + nano.faultText + "\"";
  out += "}";

  bleStatusChar->setValue(out.c_str());
  bleStatusChar->notify();
}

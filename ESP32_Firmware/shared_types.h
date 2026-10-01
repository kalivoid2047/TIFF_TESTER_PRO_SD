// Shared struct/type definitions for the TIFF_TESTER_PRO_SD ESP32 sketch.
//
// This lives in a real header (not a .ino tab) and is #included at the top
// of the main .ino, before the sketch's own function definitions. That
// matters for Arduino's automatic function-prototype generation: types used
// in a function signature must already be visible at the point the
// prototype is inserted (right after the leading #include block), which
// only works reliably if the type comes from an actual #include rather
// than from a struct defined further down inside a .ino tab.
#pragma once

#include <Arduino.h>

struct NanoStatus {
  bool relay = false;
  bool fault = false;
  bool estop = false;
  bool watchdog = false;
  float supplyV = 0;
  float dutV = 0;
  float currentA = 0;
  String faultText = "";
  // Relays 2-4 (auxiliary) and the Nano's temperature input, from the
  // Nano's EXT line. Relay 1 is `relay` above.
  bool aux[3] = {false, false, false};
  float tempC = 0;
  bool relayActiveLow = true; // from the Nano's EXT line (default active-low)
  // Active protection limits, from the Nano's LIMITS line.
  float limMinV = 11.0f, limMaxV = 15.0f, limMaxA = 5.0f;
  float limTempC = 0;        // 0 = over-temperature trip disabled
  int auxTimeoutS = 0;       // 0 = no aux relay auto-off
};

struct SystemConfig {
  String apPassword = "tifftester";
  String blePin = "TIFF2026";
  bool loadedFromSD = false;
  // MCP2515 bit timing; the crystal must match the CAN board (8 or 16 MHz).
  long canBitrate = 500000;
  int canClockMhz = 8;
};

struct ModuleProfile {
  bool loaded = false;
  String id;
  float minVoltageV = 0, maxVoltageV = 0, maxCurrentA = 0;
  float resMinOhm = 0, resMaxOhm = 0;
  bool testResistance = false;
  bool testShortToGround = false;
  bool testPositionSweep = false;
  bool testActuatorMovement = false;
  bool testCurrentMonitor = false;
  bool testPassFail = false;
  // [TEST_INJECTOR]/[TEST_COIL] (Documentation/SRS.md §6.1) — pulse-width/
  // dwell limits consumed by the Nano's INJ_TEST/COIL_TEST safety
  // supervision (see Arduino_Nano_Safety). No driver hardware to actually
  // fire yet (Documentation/ROADMAP.md Phase 1), so these are parsed and
  // reported but not yet dispatched from a test-run handler.
  bool testInjectorEnabled = false;
  float injectorDefaultPulseWidthMs = 0, injectorMaxPulseWidthMs = 0;
  int injectorDefaultDurationS = 0;
  bool testCoilEnabled = false;
  float coilDefaultDwellMs = 0, coilMaxDwellMs = 0;
  int coilDefaultDurationS = 0;
  bool serviceTurboCal = false;
  bool serviceDpfRegen = false;
  bool serviceInjectorLearn = false;
  bool serviceScvLearn = false;
  String commProtocol;
  long commBitrate = 0;
  // [COMMUNICATION] diagnostic addressing (see Documentation/DIAGNOSTICS.md)
  uint32_t canTxId = 0x7E0;
  uint32_t canRxId = 0x7E8;
  bool canExtended = false;
  long klineBaud = 10400;
  uint8_t klineTarget = 0x33;
  uint8_t klineSource = 0xF1;
  // ISO-TP padding: pad frames to 8 bytes with canPadByte, or send short
  // frames when canPadding is false. ECUs differ - see DIAGNOSTICS.md.
  uint8_t canPadByte = 0xAA;
  bool canPadding = true;
  // True only if the profile itself defined the addressing (otherwise the
  // generic defaults above are in use and unverified for this module).
  bool canIdsDefined = false;
  bool klineAddrDefined = false;
};

// One captured CAN frame for the diagnostics monitor ring buffer.
struct CanFrameRec {
  uint32_t id = 0;
  bool ext = false;
  uint8_t len = 0;
  uint8_t data[8] = {0};
};

struct TestResult {
  String testType;
  bool pass = false;
  String response;
};

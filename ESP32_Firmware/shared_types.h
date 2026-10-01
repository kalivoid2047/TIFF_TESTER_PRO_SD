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
};

struct SystemConfig {
  String apPassword = "tifftester";
  String blePin = "TIFF2026";
  bool loadedFromSD = false;
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
};

struct TestResult {
  String testType;
  bool pass = false;
  String response;
};

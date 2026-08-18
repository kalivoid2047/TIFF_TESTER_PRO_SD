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

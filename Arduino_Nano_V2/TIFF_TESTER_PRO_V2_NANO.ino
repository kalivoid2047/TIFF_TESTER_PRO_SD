/*
  TIFF_TESTER_PRO V2 - Arduino Nano output controller (I2C slave)

  Companion to ESP32_Firmware_V2/TIFF_TESTER_PRO_V2_ESP32.ino. Implements the
  I2C protocol that sketch speaks to address 0x12:

    write 0x01  GET_STATUS  - the ESP32 then reads ONE status byte back
    write 0x02  ALL_OFF     - every output off
    write 0x03  HEARTBEAT   - keeps outputs allowed (ESP32 sends it every 500 ms)
    write 0x10/0x11  relay 1 on / off      0x12/0x13  relay 2 on / off
    write 0x14/0x15  relay 3 on / off      0x16/0x17  relay 4 on / off
    write 0x20/0x21  MOSFET 1 on / off     0x22/0x23  MOSFET 2 on / off

  Status byte (never 0xFF - the ESP32 treats 0xFF as "Nano did not answer"):
    bit0..3  relay 1..4 energized     bit4..5  MOSFET 1..2 on
    bit6     fault: heartbeat lost or e-stop pressed (outputs forced off)
    bit7     always 0

  FAIL-SAFE RULES (this board only drives outputs; the ESP32 owns the tests):
    - All outputs OFF at boot, before the pins become outputs.
    - All outputs OFF if no HEARTBEAT arrives for HEARTBEAT_TIMEOUT_MS.
      (The ESP32's own timeout is 1500 ms; sending 0x03 every 500 ms keeps
      this alive.) A late heartbeat clears the fault but does NOT turn
      anything back on - the ESP32 must command it again.
    - "ON" commands are ignored until a heartbeat has been seen, while the
      heartbeat is stale, and while the e-stop is pressed.
    - Optional e-stop input on D3, FAIL-SAFE against a broken wire: wire a
      NORMALLY-CLOSED contact between D3 and GND. Pressing it, or any broken or
      unplugged wire, opens the circuit and forces everything off. (With
      ENABLE_ESTOP 1 and nothing connected the Nano sits faulted: jumper D3 to
      GND if no e-stop is fitted. See ESTOP_FAILSAFE_NC below.)
    - Hardware watchdog resets the Nano if this loop ever locks up.

  Polarity (matches the ESP32 sketch header): relay inputs are ACTIVE-LOW
  (pin LOW = relay energized); MOSFET gates are ACTIVE-HIGH.

  I2C LEVELS - READ THIS: the ESP32 is 3.3 V, the Nano is 5 V. This sketch
  turns the Nano's internal I2C pull-ups OFF so it never drives 5 V into the
  ESP32's pins. Use a bidirectional level shifter (e.g. BSS138-based) or at
  minimum external ~4.7 k pull-ups to 3.3 V; ground must be common. Do not
  connect SDA/SCL directly with 5 V pull-ups present.

  NOT validated on real hardware. Confirm relay polarity with the relay
  module before connecting anything to a DUT.
*/

#include <Arduino.h>
#include <Wire.h>
#include <avr/wdt.h>
#include <util/atomic.h>

// ---------- Configuration ----------
#define I2C_ADDRESS            0x12     // must match NANO_ADDR in the ESP32 sketch
#define HEARTBEAT_TIMEOUT_MS   1500UL
#define ENABLE_ESTOP           1        // set 0 only if you accept having NO e-stop

// E-stop wiring: 1 (default, fail-safe) = NORMALLY-CLOSED contact between D3 and
// GND, healthy = LOW, pressed OR wire broken = HIGH = e-stop. 0 (legacy) =
// normally-open contact, LOW = pressed, which does NOT detect a broken wire.
#define ESTOP_FAILSAFE_NC      1

#define RELAY_ACTIVE_LOW       1        // relay modules: LOW = energized
#define MOSFET_ACTIVE_LOW      0        // MOSFET gates: HIGH = on

// ---------- Pins (same relay pins as the main Nano firmware) ----------
// I2C is fixed by hardware: SDA = A4, SCL = A5.
#define ESTOP_PIN    3     // INPUT_PULLUP; see ESTOP_FAILSAFE_NC
#define STATUS_LED   13

const uint8_t OUTPUT_PINS[6] = {
  4,   // relay 1
  5,   // relay 2
  7,   // relay 3
  8,   // relay 4
  9,   // MOSFET 1
  10   // MOSFET 2
};

// ---------- Protocol ----------
#define CMD_GET_STATUS 0x01
#define CMD_ALL_OFF    0x02
#define CMD_HEARTBEAT  0x03
#define CMD_R1_ON      0x10   // ..0x17: relay n on = 0x10 + 2*(n-1), off = +1
#define CMD_R4_OFF     0x17
#define CMD_M1_ON      0x20   // ..0x23
#define CMD_M2_OFF     0x23

// ---------- State (shared with the I2C interrupt handlers) ----------
volatile uint8_t outputMask = 0;          // bit i = output i is on
volatile bool faultLatched = false;       // heartbeat lost or e-stop
volatile bool heartbeatSeen = false;      // at least one heartbeat since boot
volatile unsigned long lastHeartbeatMs = 0;

static bool isActiveLow(uint8_t idx) {
  return idx < 4 ? (RELAY_ACTIVE_LOW != 0) : (MOSFET_ACTIVE_LOW != 0);
}

// Physical level that means "off" / "on" for an output.
static uint8_t offLevel(uint8_t idx) { return isActiveLow(idx) ? HIGH : LOW; }
static uint8_t onLevel(uint8_t idx)  { return isActiveLow(idx) ? LOW : HIGH; }

// Pin + mask are updated together with interrupts off, because both loop()
// and the I2C interrupt handlers call this.
static void writeOutput(uint8_t idx, bool on) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    digitalWrite(OUTPUT_PINS[idx], on ? onLevel(idx) : offLevel(idx));
    if (on) outputMask |= (1 << idx);
    else    outputMask &= ~(1 << idx);
  }
}

static void allOff() {
  for (uint8_t i = 0; i < 6; i++) writeOutput(i, false);
}

static bool estopPressed() {
#if ENABLE_ESTOP
#if ESTOP_FAILSAFE_NC
  return digitalRead(ESTOP_PIN) == HIGH;   // open circuit (pressed/broken wire)
#else
  return digitalRead(ESTOP_PIN) == LOW;
#endif
#else
  return false;
#endif
}

// lastHeartbeatMs is 4 bytes and written from the I2C interrupt, so read it
// atomically (a torn read could look like a huge age and cause a false timeout).
static unsigned long lastBeat() {
  unsigned long v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = lastHeartbeatMs; }
  return v;
}

static bool heartbeatFresh() {
  return heartbeatSeen && (millis() - lastBeat()) <= HEARTBEAT_TIMEOUT_MS;
}

// Turning an output ON is only allowed while everything is healthy.
static void setOutput(uint8_t idx, bool on) {
  if (!on) { writeOutput(idx, false); return; }
  if (faultLatched || estopPressed() || !heartbeatFresh()) return;
  writeOutput(idx, true);
}

// Runs in interrupt context (Wire.onReceive): only digitalWrite and flag
// updates here - no Serial, no delay.
static void handleCommand(uint8_t cmd) {
  switch (cmd) {
    case CMD_GET_STATUS:
      return; // the status byte is returned from onRequest()

    case CMD_ALL_OFF:
      allOff();
      return;

    case CMD_HEARTBEAT:
      lastHeartbeatMs = millis();
      heartbeatSeen = true;
      // A heartbeat after a timeout clears the fault, but outputs stay off:
      // the ESP32 has to command them on again.
      if (faultLatched && !estopPressed()) faultLatched = false;
      return;
  }

  if (cmd >= CMD_R1_ON && cmd <= CMD_R4_OFF) {
    uint8_t n = (cmd - CMD_R1_ON) / 2;       // 0..3
    setOutput(n, ((cmd - CMD_R1_ON) % 2) == 0);
    return;
  }

  if (cmd >= CMD_M1_ON && cmd <= CMD_M2_OFF) {
    uint8_t n = 4 + (cmd - CMD_M1_ON) / 2;   // 4..5
    setOutput(n, ((cmd - CMD_M1_ON) % 2) == 0);
    return;
  }
  // Unknown command: ignored.
}

static uint8_t statusByte() {
  uint8_t s = outputMask & 0x3F;
  if (faultLatched) s |= 0x40;
  return s; // bit 7 is never set, so this can never be 0xFF
}

void onReceiveHandler(int numBytes) {
  // The ESP32 sends one command byte per transaction; take the first and
  // drain anything else so the buffer can't wedge.
  if (numBytes < 1) return;
  uint8_t cmd = Wire.read();
  while (Wire.available()) Wire.read();
  handleCommand(cmd);
}

void onRequestHandler() {
  Wire.write(statusByte());
}

void setup() {
  // Outputs must be safe before anything else: drive the "off" level first,
  // then switch the pin to OUTPUT, so there is no glitch at boot.
  for (uint8_t i = 0; i < 6; i++) {
    digitalWrite(OUTPUT_PINS[i], offLevel(i));
    pinMode(OUTPUT_PINS[i], OUTPUT);
  }
  allOff();

#if ENABLE_ESTOP
  pinMode(ESTOP_PIN, INPUT_PULLUP);
#endif
  pinMode(STATUS_LED, OUTPUT);

  Wire.begin(I2C_ADDRESS);
  // Wire.begin() enables the AVR's internal pull-ups to 5 V. Turn them off so
  // the 3.3 V ESP32 never sees 5 V on SDA/SCL (see the header note).
  pinMode(SDA, INPUT);
  pinMode(SCL, INPUT);
  Wire.onReceive(onReceiveHandler);
  Wire.onRequest(onRequestHandler);

  // Hardware watchdog: reset the Nano (outputs return to off) if loop() stalls.
  wdt_enable(WDTO_2S);
}

void loop() {
  wdt_reset();

  // Heartbeat supervision: no heartbeat in time -> everything off, fault set.
  if (heartbeatSeen && !heartbeatFresh()) {
    allOff();
    faultLatched = true;
  }

  // E-stop: force everything off and flag a fault while pressed.
  if (estopPressed()) {
    allOff();
    faultLatched = true;
  }

  // LED: on while any output is energized, fast blink while faulted.
  if (faultLatched) {
    digitalWrite(STATUS_LED, (millis() / 150) % 2);
  } else {
    digitalWrite(STATUS_LED, outputMask ? HIGH : LOW);
  }
}

// Minimal ISO 14230 (KWP2000) K-Line driver over a spare ESP32 UART.
//
// The L9637D on the wiring doc is just a K-Line transceiver (line driver) —
// the actual protocol lives in software, which is what this file provides:
// the ISO 14230 "fast init" wake-up sequence and basic request/response
// framing with a checksum.
//
// NOT implemented: the 5-baud "slow init" sequence used by some older
// ECUs — its timing/address behavior is ECU-specific enough that a generic
// implementation would be more likely to be wrong than helpful. Only fast
// init is provided here.
//
// IMPORTANT: this has not been validated against a real K-Line ECU. See
// Documentation/PROJECT_STATUS.md. Do not send arbitrary requests to a
// vehicle/module — this is transport-layer plumbing only, not a validated
// diagnostic sequence for any specific module.

#define KLINE_BAUD 10400

// True once the UART is open. This does NOT mean an ECU answered - the
// L9637D transceiver can't be probed from here; klineFastInit() is what
// tells you whether an ECU responds.
bool klineReady = false;

void klineInit() {
  KlineSerial.begin(KLINE_BAUD, SERIAL_8N1, KLINE_RX, KLINE_TX);
  klineReady = true;
}

// ISO 14230 fast init: K-line low for 25ms, high for 25ms, then send the
// StartCommunication request and wait for a response.
bool klineFastInit() {
  pinMode(KLINE_TX, OUTPUT);
  digitalWrite(KLINE_TX, LOW);
  delay(25);
  digitalWrite(KLINE_TX, HIGH);
  delay(25);

  KlineSerial.begin(KLINE_BAUD, SERIAL_8N1, KLINE_RX, KLINE_TX);

  // format byte, target (functional), source (tester), StartCommunication SID
  uint8_t frame[8] = {0xC1, 0x33, 0xF1, 0x81, 0, 0, 0, 0};
  uint8_t frameLen = 4;

  uint8_t checksum = 0;
  for (uint8_t i = 0; i < frameLen; i++) checksum += frame[i];
  frame[frameLen++] = checksum;

  for (uint8_t i = 0; i < frameLen; i++) KlineSerial.write(frame[i]);
  KlineSerial.flush();

  uint8_t resp[16];
  uint8_t respLen = 0;
  uint32_t start = millis();
  while (millis() - start < 300 && respLen < sizeof(resp)) {
    if (KlineSerial.available()) {
      resp[respLen++] = KlineSerial.read();
    }
  }

  return respLen > 0;
}

bool klineSendRequest(const uint8_t *data, uint8_t len) {
  if (len > 6) return false; // keep the simplified fixed-size frame below in bounds

  uint8_t frame[10];
  uint8_t idx = 0;
  frame[idx++] = 0x80 | len; // format byte: physical addressing, length in low bits
  frame[idx++] = 0x33;       // target (tester-to-ECU functional address, simplified)
  frame[idx++] = 0xF1;       // source (tester)
  for (uint8_t i = 0; i < len; i++) frame[idx++] = data[i];

  uint8_t checksum = 0;
  for (uint8_t i = 0; i < idx; i++) checksum += frame[i];
  frame[idx++] = checksum;

  for (uint8_t i = 0; i < idx; i++) KlineSerial.write(frame[i]);
  KlineSerial.flush();
  return true;
}

bool klineReadResponse(uint8_t *buf, uint8_t &len, uint32_t timeoutMs) {
  len = 0;
  uint32_t start = millis();
  while (millis() - start < timeoutMs && len < 32) {
    if (KlineSerial.available()) {
      buf[len++] = KlineSerial.read();
    }
  }
  return len > 0;
}

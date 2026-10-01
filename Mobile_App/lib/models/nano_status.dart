/// Mirrors the JSON payload the ESP32's BLE Status characteristic sends,
/// which in turn mirrors the Nano's `STATUS,...` UART line. See
/// Documentation/API_PROTOCOL_SPEC.md §1.4 and
/// ESP32_Firmware/ble_service.ino `bleNotifyStatus()`.
class NanoStatus {
  final bool relay;
  final bool fault;
  final bool estop;
  final bool watchdog;
  final double supplyV;
  final double dutV;
  final double currentA;
  final String faultText;

  /// Relay states, index 0 = relay 1 (the DUT relay) .. index 3 = relay 4.
  /// Only the BLE firmware reports relays 2-4; the V2 Bluetooth Classic
  /// firmware doesn't, so they read as off there.
  final List<bool> relays;

  /// Nano temperature input, degrees C.
  final double tempC;

  /// Position feedback, 0-100 %. Raw analog reading, not calibrated.
  final double positionPct;

  /// Subsystem readiness flags for the live-data preview.
  final bool canReady;
  final bool klineReady;
  final bool inaReady;
  final bool systemReady;

  const NanoStatus({
    required this.relay,
    required this.fault,
    required this.estop,
    required this.watchdog,
    required this.supplyV,
    required this.dutV,
    required this.currentA,
    required this.faultText,
    this.relays = const [false, false, false, false],
    this.tempC = 0,
    this.positionPct = 0,
    this.canReady = false,
    this.klineReady = false,
    this.inaReady = false,
    this.systemReady = false,
  });

  const NanoStatus.unknown()
      : relay = false,
        fault = false,
        estop = false,
        watchdog = false,
        supplyV = 0,
        dutV = 0,
        currentA = 0,
        faultText = '',
        relays = const [false, false, false, false],
        tempC = 0,
        positionPct = 0,
        canReady = false,
        klineReady = false,
        inaReady = false,
        systemReady = false;

  factory NanoStatus.fromJson(Map<String, dynamic> json) {
    return NanoStatus(
      relay: (json['relay'] ?? 0) == 1 || json['relay'] == true,
      fault: (json['fault'] ?? 0) == 1 || json['fault'] == true,
      estop: (json['estop'] ?? 0) == 1 || json['estop'] == true,
      watchdog: (json['watchdog'] ?? 0) == 1 || json['watchdog'] == true,
      supplyV: (json['supply_v'] as num? ?? 0).toDouble(),
      dutV: (json['dut_v'] as num? ?? 0).toDouble(),
      currentA: (json['current_a'] as num? ?? 0).toDouble(),
      faultText: (json['fault_text'] as String?) ?? '',
      relays: _parseRelays(json),
      tempC: (json['temp_c'] as num? ?? 0).toDouble(),
      positionPct: (json['pos_pct'] as num? ?? 0).toDouble(),
      canReady: (json['can'] ?? 0) == 1,
      klineReady: (json['kline'] ?? 0) == 1,
      inaReady: (json['ina'] ?? 0) == 1,
      systemReady: (json['sys'] ?? 0) == 1,
    );
  }

  /// Falls back to the legacy single `relay` field (relay 1 only) when the
  /// firmware predates the `relays` array.
  static List<bool> _parseRelays(Map<String, dynamic> json) {
    final raw = json['relays'];
    final out = [false, false, false, false];
    if (raw is List) {
      for (var i = 0; i < 4 && i < raw.length; i++) {
        out[i] = raw[i] == 1 || raw[i] == true;
      }
    } else {
      out[0] = (json['relay'] ?? 0) == 1 || json['relay'] == true;
    }
    return out;
  }
}

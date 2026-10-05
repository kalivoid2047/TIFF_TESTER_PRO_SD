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

  /// MOSFET output states, index 0 = MOSFET 1, index 1 = MOSFET 2. Only the
  /// V2 Bluetooth Classic firmware (V2.2.1+) has and reports them.
  final List<bool> mosfets;

  /// True when [mosfets] came from the firmware (read back from the Nano).
  final bool mosfetsReported;

  /// Nano temperature input, degrees C.
  final double tempC;

  /// Position feedback, 0-100 %. Raw analog reading, not calibrated.
  final double positionPct;

  /// Subsystem readiness flags for the live-data preview.
  final bool canReady;
  final bool klineReady;
  final bool inaReady;
  final bool systemReady;

  /// True if the relay module is configured active-low (GPIO LOW = energized),
  /// the firmware default. Applies to all four relays.
  final bool relayActiveLow;

  /// False when the firmware's temperature value isn't degrees C (the V2
  /// Bluetooth Classic firmware reports a raw ADC voltage placeholder), so
  /// the UI shows n/a instead of a misleading number.
  final bool tempReported;

  /// True when [relays] came from the firmware (BLE firmware, or V2.2.1+ which
  /// reads them back from the Nano). False on older V2 firmware, where the
  /// app can only show the last command it sent.
  final bool relaysReported;

  /// Active protection limits reported by the Nano: maximum DUT current (A),
  /// over-temperature trip (degC, 0 = disabled) and aux relay auto-off
  /// (seconds, 0 = none).
  final double limMaxA;
  final double limTempC;
  final int auxTimeoutS;

  /// Boot/requested self-test results: -1 not run, 0 failed, 1 passed.
  final int canSelfTest;
  final int klineSelfTest;

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
    this.mosfets = const [false, false],
    this.mosfetsReported = false,
    this.tempC = 0,
    this.positionPct = 0,
    this.canReady = false,
    this.klineReady = false,
    this.inaReady = false,
    this.systemReady = false,
    this.relayActiveLow = true,
    this.tempReported = true,
    this.relaysReported = true,
    this.limMaxA = 5.0,
    this.limTempC = 0,
    this.auxTimeoutS = 0,
    this.canSelfTest = -1,
    this.klineSelfTest = -1,
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
        mosfets = const [false, false],
        mosfetsReported = false,
        tempC = 0,
        positionPct = 0,
        canReady = false,
        klineReady = false,
        inaReady = false,
        systemReady = false,
        relayActiveLow = true,
        tempReported = true,
        relaysReported = true,
        limMaxA = 5.0,
        limTempC = 0,
        auxTimeoutS = 0,
        canSelfTest = -1,
        klineSelfTest = -1;

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
      relayActiveLow: (json['pol'] ?? 1) == 1,
      limMaxA: (json['lim_a'] as num? ?? 5.0).toDouble(),
      limTempC: (json['lim_t'] as num? ?? 0).toDouble(),
      auxTimeoutS: (json['aux_to'] as num? ?? 0).toInt(),
      canSelfTest: (json['can_st'] as num? ?? -1).toInt(),
      klineSelfTest: (json['kline_st'] as num? ?? -1).toInt(),
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

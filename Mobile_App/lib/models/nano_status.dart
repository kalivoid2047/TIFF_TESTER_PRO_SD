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

  const NanoStatus({
    required this.relay,
    required this.fault,
    required this.estop,
    required this.watchdog,
    required this.supplyV,
    required this.dutV,
    required this.currentA,
    required this.faultText,
  });

  const NanoStatus.unknown()
      : relay = false,
        fault = false,
        estop = false,
        watchdog = false,
        supplyV = 0,
        dutV = 0,
        currentA = 0,
        faultText = '';

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
    );
  }
}

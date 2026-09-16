import 'dart:async';
import 'dart:convert';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../models/nano_status.dart';
import '../models/test_result.dart';

/// GATT UUIDs — must match ESP32_Firmware/TIFF_TESTER_PRO_SD_ESP32.ino and
/// Documentation/API_PROTOCOL_SPEC.md §1.1/1.2 exactly.
class TiffBleUuids {
  static final service = Guid('6e400001-b5a3-f393-e0a9-e50e24dcca9e');
  static final auth = Guid('6e400002-b5a3-f393-e0a9-e50e24dcca9e');
  static final status = Guid('6e400003-b5a3-f393-e0a9-e50e24dcca9e');
  static final command = Guid('6e400004-b5a3-f393-e0a9-e50e24dcca9e');
  static final result = Guid('6e400005-b5a3-f393-e0a9-e50e24dcca9e');
  static final modules = Guid('6e400006-b5a3-f393-e0a9-e50e24dcca9e');
  static final deviceInfo = Guid('6e400007-b5a3-f393-e0a9-e50e24dcca9e');
}

/// Device name prefix the firmware advertises under (see `bleInit()` in
/// ble_service.ino: `BLEDevice::init("TiffTester")`). Used to badge
/// compatible devices in the scan list per Documentation/UI_UX_SPEC.md §4.3.
const String kTiffDeviceNamePrefix = 'TiffTester';

/// Thin wrapper around flutter_blue_plus implementing the app<->ESP32
/// contract from Documentation/API_PROTOCOL_SPEC.md. Owns exactly one
/// active connection at a time, matching the bench-tool use case (one
/// technician, one unit).
class TiffBleService {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _authChar;
  BluetoothCharacteristic? _statusChar;
  BluetoothCharacteristic? _commandChar;
  BluetoothCharacteristic? _resultChar;
  BluetoothCharacteristic? _deviceInfoChar;
  BluetoothCharacteristic? _modulesChar;

  StreamSubscription<List<int>>? _statusSub;
  StreamSubscription<List<int>>? _resultSub;
  StreamSubscription<BluetoothConnectionState>? _connectionSub;

  final _statusController = StreamController<NanoStatus>.broadcast();
  final _resultController = StreamController<TestResult>.broadcast();
  final _connectionController =
      StreamController<BluetoothConnectionState>.broadcast();

  Stream<NanoStatus> get statusStream => _statusController.stream;
  Stream<TestResult> get resultStream => _resultController.stream;
  Stream<BluetoothConnectionState> get connectionStateStream =>
      _connectionController.stream;

  BluetoothDevice? get device => _device;

  Future<void> startScan({Duration timeout = const Duration(seconds: 12)}) {
    return FlutterBluePlus.startScan(timeout: timeout);
  }

  Future<void> stopScan() => FlutterBluePlus.stopScan();

  Stream<List<ScanResult>> get scanResults => FlutterBluePlus.scanResults;

  Future<void> connect(BluetoothDevice device) async {
    _device = device;

    await device.connect(
      timeout: const Duration(seconds: 10),
      autoConnect: false,
    );

    _connectionSub?.cancel();
    _connectionSub = device.connectionState.listen((state) {
      _connectionController.add(state);
      if (state == BluetoothConnectionState.disconnected) {
        _cleanupAfterDisconnect();
      }
    });

    final services = await device.discoverServices();
    final svc = services.firstWhere(
      (s) => s.uuid == TiffBleUuids.service,
      orElse: () => throw StateError(
          'TIFF TESTER PRO BLE service not found on this device.'),
    );

    for (final c in svc.characteristics) {
      if (c.uuid == TiffBleUuids.auth) _authChar = c;
      if (c.uuid == TiffBleUuids.status) _statusChar = c;
      if (c.uuid == TiffBleUuids.command) _commandChar = c;
      if (c.uuid == TiffBleUuids.result) _resultChar = c;
      if (c.uuid == TiffBleUuids.deviceInfo) _deviceInfoChar = c;
      if (c.uuid == TiffBleUuids.modules) _modulesChar = c;
    }

    if (_statusChar != null) {
      await _statusChar!.setNotifyValue(true);
      _statusSub = _statusChar!.onValueReceived.listen((bytes) {
        _handleStatusBytes(bytes);
      });
    }

    if (_resultChar != null) {
      await _resultChar!.setNotifyValue(true);
      _resultSub = _resultChar!.onValueReceived.listen((bytes) {
        _handleResultBytes(bytes);
      });
    }
  }

  void _handleStatusBytes(List<int> bytes) {
    try {
      final text = utf8.decode(bytes);
      final json = jsonDecode(text) as Map<String, dynamic>;
      _statusController.add(NanoStatus.fromJson(json));
    } catch (_) {
      // Malformed/partial notification — drop it rather than crash the
      // app; the next ~250ms status tick will correct itself.
    }
  }

  void _handleResultBytes(List<int> bytes) {
    try {
      final text = utf8.decode(bytes);
      _resultController.add(TestResult.parse(text));
    } catch (_) {
      // Same reasoning as above.
    }
  }

  /// Writes the PIN to the Auth characteristic. The firmware doesn't ack
  /// success/failure directly on this characteristic — the *effect* is
  /// observed by whether a subsequent command actually does anything, so
  /// callers should treat "PIN accepted" optimistically and rely on the
  /// device's real behavior (e.g. ECU ON actually changing `relay` in the
  /// next status notification) as the real confirmation. See
  /// Documentation/API_PROTOCOL_SPEC.md §3 for why this is intentionally
  /// simple for now.
  Future<void> authenticate(String pin) async {
    final c = _authChar;
    if (c == null) throw StateError('Not connected.');
    await c.write(utf8.encode(pin), withoutResponse: false);
  }

  Future<void> sendCommand(String command) async {
    final c = _commandChar;
    if (c == null) throw StateError('Not connected.');
    await c.write(utf8.encode(command), withoutResponse: false);
  }

  Future<String?> readDeviceInfo() async {
    final c = _deviceInfoChar;
    if (c == null) return null;
    final bytes = await c.read();
    return utf8.decode(bytes);
  }

  /// Reads the module list characteristic and parses the firmware's
  /// "MODULES\n<file1>\n<file2>..." text (see `moduleListText()` in
  /// TIFF_TESTER_PRO_SD_ESP32.ino) into module ids — the `.INI` filename
  /// stem, matching what `SELECT_MODULE:<id>` expects.
  Future<List<String>> readModuleList() async {
    final c = _modulesChar;
    if (c == null) return const [];
    final bytes = await c.read();
    final text = utf8.decode(bytes);
    final lines = text.split('\n').map((l) => l.trim()).where((l) =>
        l.isNotEmpty && l != 'MODULES' && l.toUpperCase().endsWith('.INI'));
    return lines.map((f) => f.substring(0, f.length - 4)).toList();
  }

  Future<void> disconnect() async {
    await _device?.disconnect();
    _cleanupAfterDisconnect();
  }

  void _cleanupAfterDisconnect() {
    _statusSub?.cancel();
    _resultSub?.cancel();
    _statusSub = null;
    _resultSub = null;
    _authChar = null;
    _statusChar = null;
    _commandChar = null;
    _resultChar = null;
    _deviceInfoChar = null;
    _modulesChar = null;
  }

  void dispose() {
    _connectionSub?.cancel();
    _statusSub?.cancel();
    _resultSub?.cancel();
    _statusController.close();
    _resultController.close();
    _connectionController.close();
  }
}

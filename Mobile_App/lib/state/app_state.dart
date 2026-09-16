import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../ble/tiff_ble_service.dart';
import '../models/nano_status.dart';
import '../models/test_result.dart';

enum AppConnectionState { disconnected, connecting, connected }

/// App-wide state: connection status, live Nano status, and the current
/// session's test results. Screens read this via `Provider`/`Consumer`
/// rather than talking to `TiffBleService` directly, so BLE plumbing stays
/// in one place. See Documentation/UI_UX_SPEC.md for the screens this
/// backs.
class AppState extends ChangeNotifier {
  final TiffBleService ble = TiffBleService();

  AppConnectionState connectionState = AppConnectionState.disconnected;
  BluetoothDevice? connectedDevice;
  String? connectError;

  NanoStatus nanoStatus = const NanoStatus.unknown();
  final List<TestResult> results = [];

  /// The `.INI` filename stem of the module selected via `selectModule()`
  /// this session — null until one is picked (see Module Selection screen,
  /// Documentation/ROADMAP.md Phase 3). Not read back from the firmware, so
  /// it doesn't survive a reconnect to a device that already has a module
  /// selected from elsewhere.
  String? activeModuleId;

  /// Default PIN shown on the Connect screen (Documentation/UI_UX_SPEC.md
  /// §4.4). Matches the firmware's default in config.ino — change both if
  /// you change one.
  static const String defaultPin = 'TIFF2026';

  StreamSubscription<NanoStatus>? _statusSub;
  StreamSubscription<TestResult>? _resultSub;
  StreamSubscription<BluetoothConnectionState>? _connectionSub;

  AppState() {
    _statusSub = ble.statusStream.listen((s) {
      nanoStatus = s;
      notifyListeners();
    });

    _resultSub = ble.resultStream.listen((r) {
      results.insert(0, r); // newest first, matches Results screen (§4.10)
      notifyListeners();
      _persistResults();
    });

    _connectionSub = ble.connectionStateStream.listen((state) {
      if (state == BluetoothConnectionState.disconnected) {
        connectionState = AppConnectionState.disconnected;
        connectedDevice = null;
        nanoStatus = const NanoStatus.unknown();
        activeModuleId = null;
        notifyListeners();
      }
    });

    _restoreResults();
  }

  Future<void> connectAndAuthenticate(
      BluetoothDevice device, String pin) async {
    connectionState = AppConnectionState.connecting;
    connectError = null;
    notifyListeners();

    try {
      await ble.connect(device);
      await ble.authenticate(pin);
      connectedDevice = device;
      connectionState = AppConnectionState.connected;
      await _rememberLastDevice(device.remoteId.str);
    } catch (e) {
      connectError = e.toString();
      connectionState = AppConnectionState.disconnected;
      rethrow;
    } finally {
      notifyListeners();
    }
  }

  Future<void> disconnect() async {
    await ble.disconnect();
    connectionState = AppConnectionState.disconnected;
    connectedDevice = null;
    notifyListeners();
  }

  bool get isConnected => connectionState == AppConnectionState.connected;

  Future<void> powerOn() => ble.sendCommand('POWER_ON');
  Future<void> powerOff() => ble.sendCommand('POWER_OFF');
  Future<void> resetFault() => ble.sendCommand('RESET_FAULT');

  Future<void> selectModule(String moduleId) async {
    await ble.sendCommand('SELECT_MODULE:$moduleId');
    activeModuleId = moduleId;
    notifyListeners();
  }

  Future<List<String>> fetchModuleList() => ble.readModuleList();

  Future<void> runQuickTest(String testName) =>
      ble.sendCommand('RUN_TEST:$testName');

  Future<void> runInjectorTest({
    required int channel,
    required double pulseWidthMs,
    required int durationS,
  }) =>
      ble.sendCommand(
          'RUN_INJECTOR_TEST:$channel,$pulseWidthMs,$durationS');

  Future<void> runCoilTest({
    required int channel,
    required double dwellMs,
    required int durationS,
  }) =>
      ble.sendCommand('RUN_COIL_TEST:$channel,$dwellMs,$durationS');

  Future<void> runAllInjectors({
    required double pulseWidthMs,
    required int durationPerS,
  }) =>
      ble.sendCommand('RUN_ALL_INJECTORS:$pulseWidthMs,$durationPerS');

  Future<void> stopTest() => ble.sendCommand('STOP_TEST');

  Future<void> setPin(String newPin) => ble.sendCommand('SET_PIN:$newPin');

  Future<void> forgetLastDevice() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('last_device_id');
  }

  void clearResults() {
    results.clear();
    notifyListeners();
    _persistResults();
  }

  Future<void> _rememberLastDevice(String remoteId) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('last_device_id', remoteId);
  }

  Future<String?> get lastDeviceId async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString('last_device_id');
  }

  /// Results are capped at this many entries locally — enough for a full
  /// bench session's history without the persisted blob growing unbounded.
  static const int _maxStoredResults = 200;

  Future<void> _persistResults() async {
    final prefs = await SharedPreferences.getInstance();
    final stored = results.take(_maxStoredResults).toList();
    await prefs.setString(
      'stored_results',
      jsonEncode(stored.map((r) => r.toJson()).toList()),
    );
  }

  Future<void> _restoreResults() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString('stored_results');
    if (raw == null) return;

    try {
      final decoded = jsonDecode(raw) as List<dynamic>;
      results
        ..clear()
        ..addAll(decoded
            .map((e) => TestResult.fromJson(e as Map<String, dynamic>)));
      notifyListeners();
    } catch (_) {
      // Corrupt/old-format blob (e.g. the previous int-only format) — start
      // fresh rather than crashing the app on launch.
    }
  }

  @override
  void dispose() {
    _statusSub?.cancel();
    _resultSub?.cancel();
    _connectionSub?.cancel();
    ble.dispose();
    super.dispose();
  }
}

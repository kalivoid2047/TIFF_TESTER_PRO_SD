import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../ble/tiff_ble_service.dart';
import '../models/diag_line.dart';
import '../models/module_profile.dart';
import '../models/nano_status.dart';
import '../models/test_result.dart';
import '../models/vehicle.dart';

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

  /// Display name/id of the connected device, for both BLE and the V2
  /// Bluetooth Classic link (where `connectedDevice` is null).
  String? connectedName;
  String? connectedId;
  String? connectError;

  /// True if the currently-connected device was authenticated with the
  /// still-shipped default PIN (`defaultPin` below) — a nudge to change it
  /// before field use (Documentation/ROADMAP.md Phase 4 "default-PIN
  /// warning"; mirrors the firmware's own `ble_pin_is_default` flag on the
  /// Wi-Fi debug API, which the BLE-only app has no way to read directly).
  bool usingDefaultPin = false;

  NanoStatus nanoStatus = const NanoStatus.unknown();
  final List<TestResult> results = [];

  /// Local vehicle/module databases (Documentation/PRD.md §11, vision brief
  /// §13–§16). Purely offline metadata the technician maintains on the
  /// phone — independent of the BLE connection and the SD-card module list
  /// (`fetchModuleList()`/`selectModule()` below), which come from the
  /// firmware instead.
  final List<Vehicle> vehicles = [];
  final List<ModuleProfile> moduleProfiles = [];

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
  StreamSubscription<DiagLine>? _diagSub;
  StreamSubscription<BluetoothConnectionState>? _connectionSub;

  /// Diagnostics console history (newest last), capped so a long CAN monitor
  /// session can't grow without bound.
  final List<DiagLine> diagLog = [];
  static const int _maxDiagLines = 400;

  AppState() {
    _diagSub = ble.diagStream.listen((l) {
      _addDiag(l);
    });

    _statusSub = ble.statusStream.listen((s) {
      nanoStatus = s;
      // V2 drops every output on a fault, so the last-commanded states are stale.
      if (s.fault && ble.isClassic) {
        for (var i = 0; i < _classicRelays.length; i++) {
          _classicRelays[i] = false;
        }
      }
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
        connectedName = null;
        connectedId = null;
        nanoStatus = const NanoStatus.unknown();
        for (var i = 0; i < _classicRelays.length; i++) {
          _classicRelays[i] = false;
        }
        activeModuleId = null;
        usingDefaultPin = false;
        notifyListeners();
      }
    });

    _restoreResults();
    _restoreVehicles();
    _restoreModuleProfiles();
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
      connectedName = device.platformName;
      connectedId = device.remoteId.str;
      connectionState = AppConnectionState.connected;
      usingDefaultPin = pin == defaultPin;
      await _rememberLastDevice(device.remoteId.str);
    } catch (e) {
      connectError = e.toString();
      connectionState = AppConnectionState.disconnected;
      rethrow;
    } finally {
      notifyListeners();
    }
  }

  /// Connects to a paired V2 (Bluetooth Classic) board — no PIN.
  Future<void> connectClassic(String address, String name) async {
    connectionState = AppConnectionState.connecting;
    connectError = null;
    notifyListeners();

    try {
      await ble.connectClassic(address, name);
      connectedDevice = null;
      connectedName = name;
      connectedId = address;
      connectionState = AppConnectionState.connected;
      usingDefaultPin = false;
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
    connectedName = null;
    connectedId = null;
    notifyListeners();
  }

  bool get isConnected => connectionState == AppConnectionState.connected;

  /// True when connected over the V2 Bluetooth Classic link. That firmware
  /// only reports voltage/current/relay state and has no auxiliary relays,
  /// polarity control or diagnostics engine.
  bool get isClassic => ble.isClassic;

  /// Last relay state *commanded* over the V2 link. Only used with older V2
  /// firmware that doesn't report relay state; V2.2.1+ reports it read back
  /// from the Nano (see [relayStates]).
  final List<bool> _classicRelays = [false, false, false, false];

  /// Relay states for the UI: reported by the firmware where it can (BLE
  /// firmware, V2.2.1+); the last command sent on older V2 firmware.
  List<bool> get relayStates => (isClassic && !nanoStatus.relaysReported)
      ? List.unmodifiable(_classicRelays)
      : nanoStatus.relays;

  /// True when the relay states shown are the last command sent rather than
  /// a reading (older V2 firmware only).
  bool get relayStatesAreCommanded => isClassic && !nanoStatus.relaysReported;

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

  /// Switches relay [n] (1 = DUT relay, 2-4 = auxiliary). The Nano decides
  /// whether to honor it (e-stop/fault/supply gating); the real state comes
  /// back in the next status notification.
  Future<void> setRelay(int n, bool on) async {
    await ble.sendCommand('RELAY:$n,${on ? 1 : 0}');
    if (isClassic && n >= 1 && n <= 4) {
      _classicRelays[n - 1] = on;
      notifyListeners();
    }
  }

  /// Cycles relay [n] on/off 3x on the Nano; the outcome arrives as a
  /// `relay_<n>` entry on the Results screen.
  Future<void> testRelay(int n) => ble.sendCommand('RELAY_TEST:$n');

  Future<void> stopTest() => ble.sendCommand('STOP_TEST');

  // --- Diagnostics (CAN / UDS / K-Line / KWP) ---------------------------
  // The ESP32 owns the transport (ISO-TP, KWP framing, safety whitelists);
  // the app only sends text commands and shows what comes back. See
  // Documentation/DIAGNOSTICS.md. The V2 Bluetooth Classic firmware has no
  // equivalent, so these throw there (the screen shows the message).

  void _addDiag(DiagLine l) {
    diagLog.add(l);
    if (diagLog.length > _maxDiagLines) {
      diagLog.removeRange(0, diagLog.length - _maxDiagLines);
    }
    notifyListeners();
  }

  void clearDiagLog() {
    diagLog.clear();
    notifyListeners();
  }

  /// Sends a diagnostics command and echoes it into the console.
  Future<void> sendDiag(String command) async {
    await ble.sendCommand(command);
    _addDiag(DiagLine(
        kind: 'tx',
        ok: true,
        text: command,
        time: DateTime.now(),
        sent: true));
  }

  /// Stops monitors and drops any queued diagnostics (always allowed, even
  /// before PIN auth).
  Future<void> diagStop() => ble.sendCommand('DIAG_STOP');

  /// Relay polarity for all four relays: true = active-low (the default).
  /// The Nano forces every relay off before and after the change.
  Future<void> setRelayPolarity({required bool activeLow}) =>
      ble.sendCommand('SET_POLARITY:${activeLow ? 1 : 0}');

  /// Emergency "ALL OUTPUTS OFF" (vision brief §18, §37) — deliberately
  /// distinct from a normal `powerOff()`: sends both `POWER_OFF` (drops the
  /// DUT relay) and `STOP_TEST` (aborts any in-flight channel test), the
  /// two commands `ble_service.ino` always allows regardless of PIN auth
  /// state (safety-favorable). Only the DUT relay physically exists today
  /// (Documentation/ROADMAP.md Phase 1 — no auxiliary relay/MOSFET driver
  /// hardware yet), so this is everything that actually can be turned off.
  Future<void> allOutputsOff() async {
    await ble.sendCommand('POWER_OFF');
    await ble.sendCommand('STOP_TEST');
    _clearClassicRelays();
  }

  void _clearClassicRelays() {
    for (var i = 0; i < _classicRelays.length; i++) {
      _classicRelays[i] = false;
    }
    notifyListeners();
  }

  Future<void> setPin(String newPin) async {
    await ble.sendCommand('SET_PIN:$newPin');
    usingDefaultPin = newPin == defaultPin;
    notifyListeners();
  }

  Future<void> forgetLastDevice() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('last_device_id');
  }

  void clearResults() {
    results.clear();
    notifyListeners();
    _persistResults();
  }

  // --- Vehicle database (PRD.md §11 / vision brief §15) ---------------

  String _newId() => DateTime.now().microsecondsSinceEpoch.toString();

  void addVehicle(Vehicle vehicle) {
    vehicles.add(vehicle);
    notifyListeners();
    _persistVehicles();
  }

  void updateVehicle(Vehicle vehicle) {
    final i = vehicles.indexWhere((v) => v.id == vehicle.id);
    if (i == -1) return;
    vehicles[i] = vehicle;
    notifyListeners();
    _persistVehicles();
  }

  void deleteVehicle(String id) {
    vehicles.removeWhere((v) => v.id == id);
    notifyListeners();
    _persistVehicles();
  }

  Vehicle duplicateVehicle(Vehicle vehicle) {
    final copy = vehicle.copyWith(model: '${vehicle.model} (copy)');
    final withNewId = Vehicle(
      id: _newId(),
      manufacturer: copy.manufacturer,
      model: copy.model,
      year: copy.year,
      engine: copy.engine,
      fuelType: copy.fuelType,
      engineCode: copy.engineCode,
      ecuInfo: copy.ecuInfo,
      protocol: copy.protocol,
      notes: copy.notes,
    );
    addVehicle(withNewId);
    return withNewId;
  }

  Future<void> _persistVehicles() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(
      'stored_vehicles',
      jsonEncode(vehicles.map((v) => v.toJson()).toList()),
    );
  }

  Future<void> _restoreVehicles() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString('stored_vehicles');
    if (raw == null) return;
    try {
      final decoded = jsonDecode(raw) as List<dynamic>;
      vehicles
        ..clear()
        ..addAll(
            decoded.map((e) => Vehicle.fromJson(e as Map<String, dynamic>)));
      notifyListeners();
    } catch (_) {
      // Corrupt/old-format blob — start fresh rather than crashing on launch.
    }
  }

  // --- Module database (PRD.md §11 / vision brief §13–§14) ------------

  void addModuleProfile(ModuleProfile module) {
    moduleProfiles.add(module);
    notifyListeners();
    _persistModuleProfiles();
  }

  void updateModuleProfile(ModuleProfile module) {
    final i = moduleProfiles.indexWhere((m) => m.id == module.id);
    if (i == -1) return;
    moduleProfiles[i] = module;
    notifyListeners();
    _persistModuleProfiles();
  }

  void deleteModuleProfile(String id) {
    moduleProfiles.removeWhere((m) => m.id == id);
    notifyListeners();
    _persistModuleProfiles();
  }

  ModuleProfile duplicateModuleProfile(ModuleProfile module) {
    final copy = module.copyWith(name: '${module.name} (copy)');
    final withNewId = ModuleProfile(
      id: _newId(),
      name: copy.name,
      vehicleId: copy.vehicleId,
      moduleType: copy.moduleType,
      communicationProtocol: copy.communicationProtocol,
      canSpeed: copy.canSpeed,
      canTxId: copy.canTxId,
      canRxId: copy.canRxId,
      klineBaud: copy.klineBaud,
      canExtended: copy.canExtended,
      canPadding: copy.canPadding,
      klineTarget: copy.klineTarget,
      klineSource: copy.klineSource,
      minVoltage: copy.minVoltage,
      maxVoltage: copy.maxVoltage,
      maxCurrent: copy.maxCurrent,
      positionMin: copy.positionMin,
      positionMax: copy.positionMax,
      tempMin: copy.tempMin,
      tempMax: copy.tempMax,
      relayRequirements: copy.relayRequirements,
      mosfetRequirements: copy.mosfetRequirements,
      testProcedure: copy.testProcedure,
      diagnosticCommands: copy.diagnosticCommands,
      passCriteria: copy.passCriteria,
      failCriteria: copy.failCriteria,
      notes: copy.notes,
      sdModuleId: copy.sdModuleId,
    );
    addModuleProfile(withNewId);
    return withNewId;
  }

  Future<void> _persistModuleProfiles() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(
      'stored_module_profiles',
      jsonEncode(moduleProfiles.map((m) => m.toJson()).toList()),
    );
  }

  Future<void> _restoreModuleProfiles() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString('stored_module_profiles');
    if (raw == null) return;
    try {
      final decoded = jsonDecode(raw) as List<dynamic>;
      moduleProfiles
        ..clear()
        ..addAll(decoded
            .map((e) => ModuleProfile.fromJson(e as Map<String, dynamic>)));
      notifyListeners();
    } catch (_) {
      // Corrupt/old-format blob — start fresh rather than crashing on launch.
    }
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
    _diagSub?.cancel();
    _connectionSub?.cancel();
    ble.dispose();
    super.dispose();
  }
}

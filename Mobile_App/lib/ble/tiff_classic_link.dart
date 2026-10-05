import 'dart:async';

import 'package:flutter_bluetooth_classic_serial/flutter_bluetooth_classic.dart'
    as bt;

import '../models/nano_status.dart';

/// Connect failure whose toString() is just the human-readable message.
class ClassicConnectException implements Exception {
  final String message;
  ClassicConnectException(this.message);
  @override
  String toString() => message;
}

/// Bluetooth Classic (SPP/RFCOMM) link for the V2.x "Bluetooth-only"
/// firmware (`ESP32_Firmware_V2/TIFF_TESTER_PRO_V2_ESP32.ino`, advertised as
/// `TIFF_TESTER_V2`; see ESP32_Firmware_V2/README.md).
/// That firmware speaks newline-terminated text commands (`STATUS`,
/// `POWER_ON`, `LIST_MODULES`, ...) instead of the BLE GATT contract in
/// Documentation/API_PROTOCOL_SPEC.md, so this class translates between the
/// two so the rest of the app doesn't care which one is connected.
class TiffClassicLink {
  final bt.FlutterBluetoothClassic _bt = bt.FlutterBluetoothClassic();

  StreamSubscription<bt.BluetoothData>? _dataSub;
  StreamSubscription<bt.BluetoothConnectionState>? _connSub;
  Timer? _pollTimer;

  final _statusController = StreamController<NanoStatus>.broadcast();
  final _disconnectedController = StreamController<void>.broadcast();
  final _messageController = StreamController<String>.broadcast();

  String _rxBuffer = '';
  Completer<List<String>>? _moduleListWaiter;
  List<String> _moduleListLines = [];

  String? deviceAddress;
  String? deviceName;
  bool connected = false;

  Stream<NanoStatus> get statusStream => _statusController.stream;
  Stream<void> get disconnectedStream => _disconnectedController.stream;

  /// Firmware replies that aren't status lines (`OK|...`, `ERROR|...`,
  /// `POWER_ON=BLOCKED`, `PRETEST=FAIL|...`) — for UI feedback.
  Stream<String> get messageStream => _messageController.stream;

  /// Paired classic devices; the V2 board must be paired in Android
  /// Bluetooth settings first.
  Future<List<bt.BluetoothDevice>> pairedDevices() => _bt.getPairedDevices();

  Future<void> connect(String address, String name) async {
    // The plugin's connect() returns true as soon as its socket thread starts;
    // whether the board actually answered arrives later on
    // onConnectionChanged (CONNECTED, or ERROR: ... e.g. page timeout when the
    // board is off or out of range). Wait for that, otherwise the app reports
    // "connected" with no link and then "drops" a few seconds later.
    final outcome = Completer<bt.BluetoothConnectionState>();
    _connSub = _bt.onConnectionChanged.listen((c) {
      if (c.deviceAddress.toUpperCase() != address.toUpperCase()) return;
      if (!outcome.isCompleted) {
        outcome.complete(c);
      } else if (!c.isConnected && connected) {
        _handleLost();
      }
    });

    try {
      final started = await _bt.connect(address);
      if (!started) {
        throw ClassicConnectException(
            'Could not open a Bluetooth serial link to $name.');
      }
      final state = await outcome.future.timeout(
        const Duration(seconds: 20),
        onTimeout: () => throw ClassicConnectException('$name did not respond.'),
      );
      if (!state.isConnected) {
        throw ClassicConnectException(
            '$name did not accept the connection (${state.status}).');
      }
    } catch (e) {
      _connSub?.cancel();
      _connSub = null;
      try {
        await _bt.disconnect();
      } catch (_) {}
      throw ClassicConnectException('$e Make sure the board is powered, in '
          'range and not connected to another device.');
    }

    deviceAddress = address;
    deviceName = name;
    connected = true;
    _rxBuffer = '';

    _dataSub = _bt.onDataReceived.listen((d) => _onData(d.asString()));

    _pollTimer =
        Timer.periodic(const Duration(milliseconds: 500), (_) => _poll());
    _poll();
  }

  void _poll() {
    if (!connected) return;
    // A failed send (link just dropped) is reported via onConnectionChanged;
    // don't let it surface as an unhandled async error every tick.
    _bt.sendString('STATUS\n').catchError((_) => false);
  }

  Future<void> sendLine(String line) async {
    if (!connected) throw StateError('Not connected.');
    await _bt.sendString('$line\n');
  }

  /// Sends `LIST_MODULES` and collects `MODULE|<file>` lines until
  /// `MODULE_LIST_END`.
  Future<List<String>> listModules() async {
    _moduleListLines = [];
    final waiter = _moduleListWaiter = Completer<List<String>>();
    await sendLine('LIST_MODULES');
    return waiter.future.timeout(const Duration(seconds: 5), onTimeout: () {
      _moduleListWaiter = null;
      return _moduleListLines;
    });
  }

  void _onData(String chunk) {
    _rxBuffer += chunk;
    int nl;
    while ((nl = _rxBuffer.indexOf('\n')) >= 0) {
      final line = _rxBuffer.substring(0, nl).trim();
      _rxBuffer = _rxBuffer.substring(nl + 1);
      if (line.isNotEmpty) _onLine(line);
    }
    if (_rxBuffer.length > 2000) _rxBuffer = '';
  }

  void _onLine(String line) {
    if (line.startsWith('VOLTAGE=')) {
      _statusController.add(_parseStatus(line));
    } else if (line == 'MODULE_LIST_END') {
      _moduleListWaiter?.complete(_moduleListLines);
      _moduleListWaiter = null;
    } else if (line.startsWith('MODULE|')) {
      _moduleListLines.add(line.substring('MODULE|'.length));
    } else if (line == 'MODULE_LIST_BEGIN') {
      _moduleListLines = [];
    } else if (line.startsWith('FAULT|')) {
      _statusController.add(NanoStatus(
        relay: false,
        fault: true,
        estop: false,
        watchdog: true,
        supplyV: 0,
        dutV: 0,
        currentA: 0,
        faultText: line.substring('FAULT|'.length),
      ));
      _messageController.add(line);
    } else if (line.startsWith('OK|') ||
        line.startsWith('ERROR|') ||
        line.contains('=BLOCKED') ||
        line.startsWith('PRETEST=') ||
        line.startsWith('VALIDATION=')) {
      _messageController.add(line);
    }
  }

  /// Parses `VOLTAGE=12.10,DUT_VOLTAGE=...,CURRENT=...,DUT=ON,FAULT=NO,...`
  /// (see `liveStatus()` in the V2 firmware).
  NanoStatus _parseStatus(String line) {
    final kv = <String, String>{};
    for (final part in line.split(',')) {
      final i = part.indexOf('=');
      if (i > 0) kv[part.substring(0, i)] = part.substring(i + 1);
    }
    double num_(String k) => double.tryParse(kv[k] ?? '') ?? 0;
    final fault = kv['FAULT'] ?? 'NO';
    return NanoStatus(
      relay: kv['DUT'] == 'ON',
      fault: fault != 'NO',
      estop: false,
      watchdog: (kv['WATCHDOG'] ?? 'OK') == 'OK',
      supplyV: num_('VOLTAGE'),
      dutV: num_('DUT_VOLTAGE'),
      currentA: num_('CURRENT'),
      faultText: fault == 'NO' ? '' : fault,
      systemReady: fault == 'NO' && (kv['WATCHDOG'] ?? 'OK') == 'OK',
      positionPct: num_('POSITION'),
      canReady: kv['CAN'] == 'READY',
      klineReady: kv['KLINE'] == 'READY',
      inaReady: kv['INA219'] == 'READY',
      // V2's TEMP is a raw ADC voltage placeholder, not degrees C.
      tempReported: false,
      // V2.2.1+ reports R1..R4 read back from the Nano; older V2.2 doesn't.
      relays: [
        kv['R1'] == 'ON',
        kv['R2'] == 'ON',
        kv['R3'] == 'ON',
        kv['R4'] == 'ON',
      ],
      relaysReported: kv.containsKey('R1'),
    );
  }

  void _handleLost() {
    _cleanup();
    _disconnectedController.add(null);
  }

  void _cleanup() {
    connected = false;
    _pollTimer?.cancel();
    _pollTimer = null;
    _dataSub?.cancel();
    _connSub?.cancel();
    _dataSub = null;
    _connSub = null;
    _moduleListWaiter?.complete(_moduleListLines);
    _moduleListWaiter = null;
  }

  Future<void> disconnect() async {
    final was = connected;
    _cleanup();
    if (was) {
      try {
        await _bt.disconnect();
      } catch (_) {}
      _disconnectedController.add(null);
    }
  }

  void dispose() {
    _cleanup();
    _statusController.close();
    _disconnectedController.close();
    _messageController.close();
  }
}

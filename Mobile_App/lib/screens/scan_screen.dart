import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:provider/provider.dart';

import '../ble/tiff_ble_service.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';
import 'connect_screen.dart';
import 'root_shell.dart';

/// Documentation/UI_UX_SPEC.md §4.3.
class ScanScreen extends StatefulWidget {
  const ScanScreen({super.key});

  @override
  State<ScanScreen> createState() => _ScanScreenState();
}

class _ScanScreenState extends State<ScanScreen> {
  bool _scanning = false;
  bool _showUnnamed = false;
  List<ScanResult> _results = [];
  // Paired Bluetooth Classic boards (V2 firmware, e.g. TIFF_TESTER_V2).
  List<({String name, String address})> _classic = [];
  String? _classicError;
  bool _connectingClassic = false;
  StreamSubscription<List<ScanResult>>? _sub;

  @override
  void initState() {
    super.initState();
    _startScan();
  }

  Future<void> _loadClassic() async {
    try {
      final paired = await context.read<AppState>().ble.classic.pairedDevices();
      if (!mounted) return;
      setState(() {
        _classic = [
          for (final d in paired)
            if (d.name.toUpperCase().contains('TIFF'))
              (name: d.name, address: d.address)
        ];
      });
    } catch (e) {
      if (mounted) setState(() => _classicError = e.toString());
    }
  }

  Future<void> _connectClassic(({String name, String address}) d) async {
    setState(() {
      _connectingClassic = true;
      _classicError = null;
    });
    final app = context.read<AppState>();
    try {
      await app.ble.stopScan();
      await app.connectClassic(d.address, d.name);
      if (!mounted) return;
      Navigator.of(context).pushAndRemoveUntil(
        MaterialPageRoute(builder: (_) => const RootShell()),
        (route) => false,
      );
    } catch (e) {
      if (mounted) setState(() => _classicError = 'Could not connect: $e');
    } finally {
      if (mounted) setState(() => _connectingClassic = false);
    }
  }

  Future<void> _startScan() async {
    final granted = await _ensurePermissions();
    if (!granted) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(
            content: Text('Bluetooth/location permission is required to scan.')));
      }
      return;
    }
    if (!mounted) return;
    _loadClassic();

    final app = context.read<AppState>();
    _sub?.cancel();
    _sub = app.ble.scanResults.listen((results) {
      bool isTiff(ScanResult r) =>
          r.advertisementData.advName.startsWith(kTiffDeviceNamePrefix) ||
          r.advertisementData.serviceUuids.contains(TiffBleUuids.service);
      final sorted = [...results]..sort((a, b) {
          if (isTiff(a) != isTiff(b)) return isTiff(a) ? -1 : 1;
          return b.rssi.compareTo(a.rssi);
        });
      setState(() => _results = sorted);
    });

    setState(() => _scanning = true);
    await app.ble.startScan();
    if (mounted) setState(() => _scanning = false);
  }

  Future<bool> _ensurePermissions() async {
    final statuses = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ].request();
    // Location is only declared for Android <=11 (manifest maxSdkVersion=30);
    // on Android 12+ it can never be granted, so only the Bluetooth
    // permissions gate scanning.
    bool ok(Permission p) => statuses[p]?.isGranted ?? false;
    return ok(Permission.bluetoothScan) && ok(Permission.bluetoothConnect);
  }

  Future<void> _stopScan() async {
    await context.read<AppState>().ble.stopScan();
    setState(() => _scanning = false);
  }

  @override
  void dispose() {
    _sub?.cancel();
    context.read<AppState>().ble.stopScan();
    super.dispose();
  }

  // platformName is frequently empty during an Android scan; the advertised
  // name lives in advertisementData.
  String _nameOf(ScanResult r) => r.advertisementData.advName.isNotEmpty
      ? r.advertisementData.advName
      : r.device.platformName;

  bool _isTiff(ScanResult r) =>
      _nameOf(r).startsWith(kTiffDeviceNamePrefix) ||
      r.advertisementData.serviceUuids.contains(TiffBleUuids.service);

  @override
  Widget build(BuildContext context) {
    final visible = _showUnnamed
        ? _results
        : _results
            .where((r) => _nameOf(r).isNotEmpty || _isTiff(r))
            .toList();
    return Scaffold(
      appBar: AppBar(title: const Text('SCAN DEVICES')),
      body: Column(
        children: [
          if (_scanning)
            const Padding(
              padding: EdgeInsets.all(24),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(Icons.bluetooth_searching, color: AppColors.brandRed),
                  SizedBox(width: 12),
                  Text('Scanning for BLE devices...'),
                  SizedBox(width: 12),
                  SizedBox(
                    width: 16,
                    height: 16,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  ),
                ],
              ),
            ),
          if (_classic.isNotEmpty || _classicError != null)
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 0),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text('PAIRED (BLUETOOTH CLASSIC)',
                      style: TextStyle(
                          color: AppColors.textSecondary, fontSize: 12)),
                  for (final d in _classic)
                    ListTile(
                      contentPadding: EdgeInsets.zero,
                      leading: const Icon(Icons.bluetooth,
                          color: AppColors.success),
                      title: Text(d.name,
                          style:
                              const TextStyle(fontWeight: FontWeight.bold)),
                      subtitle: Text(d.address,
                          style: const TextStyle(
                              color: AppColors.textSecondary)),
                      trailing: _connectingClassic
                          ? const SizedBox(
                              width: 16,
                              height: 16,
                              child:
                                  CircularProgressIndicator(strokeWidth: 2))
                          : const Icon(Icons.chevron_right),
                      onTap:
                          _connectingClassic ? null : () => _connectClassic(d),
                    ),
                  if (_classicError != null)
                    Text(_classicError!,
                        style: const TextStyle(color: AppColors.danger)),
                ],
              ),
            ),
          SwitchListTile(
            dense: true,
            title: const Text('Show unnamed devices'),
            value: _showUnnamed,
            onChanged: (v) => setState(() => _showUnnamed = v),
          ),
          Expanded(
            child: visible.isEmpty
                ? const Center(
                    child: Text('No devices found yet.',
                        style: TextStyle(color: AppColors.textSecondary)))
                : ListView.builder(
                    itemCount: visible.length,
                    itemBuilder: (context, i) {
                      final r = visible[i];
                      final name = _nameOf(r);
                      final isTiff = _isTiff(r);
                      return ListTile(
                        title: Text(
                          name.isNotEmpty
                              ? name
                              : (isTiff ? kTiffDeviceNamePrefix : '(unnamed device)'),
                          style: const TextStyle(fontWeight: FontWeight.bold),
                        ),
                        subtitle: Text(r.device.remoteId.str,
                            style:
                                const TextStyle(color: AppColors.textSecondary)),
                        trailing: Column(
                          mainAxisAlignment: MainAxisAlignment.center,
                          crossAxisAlignment: CrossAxisAlignment.end,
                          children: [
                            Text('RSSI ${r.rssi}',
                                style: const TextStyle(
                                    color: AppColors.textSecondary,
                                    fontSize: 12)),
                            Icon(Icons.signal_cellular_alt,
                                size: 16,
                                color: isTiff
                                    ? AppColors.success
                                    : AppColors.textSecondary),
                          ],
                        ),
                        onTap: () {
                          Navigator.of(context).push(MaterialPageRoute(
                            builder: (_) => ConnectScreen(device: r.device),
                          ));
                        },
                      );
                    },
                  ),
          ),
          Padding(
            padding: const EdgeInsets.all(16),
            child: ElevatedButton(
              onPressed: _scanning ? _stopScan : _startScan,
              child: Text(_scanning ? 'STOP SCAN' : 'SCAN FOR DEVICES'),
            ),
          ),
        ],
      ),
    );
  }
}

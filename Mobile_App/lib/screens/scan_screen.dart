import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:provider/provider.dart';

import '../ble/tiff_ble_service.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';
import 'connect_screen.dart';

/// Documentation/UI_UX_SPEC.md §4.3.
class ScanScreen extends StatefulWidget {
  const ScanScreen({super.key});

  @override
  State<ScanScreen> createState() => _ScanScreenState();
}

class _ScanScreenState extends State<ScanScreen> {
  bool _scanning = false;
  List<ScanResult> _results = [];
  StreamSubscription<List<ScanResult>>? _sub;

  @override
  void initState() {
    super.initState();
    _startScan();
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

  @override
  Widget build(BuildContext context) {
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
          Expanded(
            child: _results.isEmpty
                ? const Center(
                    child: Text('No devices found yet.',
                        style: TextStyle(color: AppColors.textSecondary)))
                : ListView.builder(
                    itemCount: _results.length,
                    itemBuilder: (context, i) {
                      final r = _results[i];
                      // platformName is frequently empty during an Android
                      // scan; the advertised name lives in advertisementData.
                      final name = r.advertisementData.advName.isNotEmpty
                          ? r.advertisementData.advName
                          : r.device.platformName;
                      final isTiff =
                          name.startsWith(kTiffDeviceNamePrefix) ||
                              r.advertisementData.serviceUuids
                                  .contains(TiffBleUuids.service);
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

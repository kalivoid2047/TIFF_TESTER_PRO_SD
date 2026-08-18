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
      setState(() => _results = results);
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
    return statuses.values.every((s) => s.isGranted || s.isLimited);
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
                      final isTiff = r.device.platformName
                          .startsWith(kTiffDeviceNamePrefix);
                      return ListTile(
                        title: Text(
                          r.device.platformName.isNotEmpty
                              ? r.device.platformName
                              : '(unnamed device)',
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

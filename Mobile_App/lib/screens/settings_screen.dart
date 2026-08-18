import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../widgets/app_card.dart';

/// Documentation/UI_UX_SPEC.md §6 item 2 — no reference design supplied;
/// covers the minimum from Documentation/SRS.md FR-SYS-2: change PIN,
/// forget device, clear app data.
class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _newPinController = TextEditingController();
  bool _busy = false;

  @override
  void dispose() {
    _newPinController.dispose();
    super.dispose();
  }

  Future<void> _changePin(AppState app) async {
    final pin = _newPinController.text;
    if (pin.length < 6) {
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('PIN must be at least 6 characters.')));
      return;
    }
    setState(() => _busy = true);
    try {
      await app.setPin(pin);
      _newPinController.clear();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('PIN updated on the device.')));
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('Failed to update PIN: $e')));
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _forgetDevice(AppState app) async {
    await app.forgetLastDevice();
    if (app.isConnected) await app.disconnect();
    if (mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('Device forgotten.')));
    }
  }

  Future<void> _clearAppData(AppState app) async {
    app.clearResults();
    await app.forgetLastDevice();
    if (mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('App data cleared.')));
    }
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();

    return Scaffold(
      appBar: AppBar(title: const Text('SETTINGS')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          AppCard(
            title: 'BLE Pairing PIN',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                if (!app.isConnected)
                  const Text(
                    'Connect to a device first to change its PIN.',
                    style: TextStyle(color: AppColors.textSecondary),
                  )
                else ...[
                  TextField(
                    controller: _newPinController,
                    decoration:
                        const InputDecoration(hintText: 'New PIN (min 6 chars)'),
                  ),
                  const SizedBox(height: 12),
                  ElevatedButton(
                    onPressed: _busy ? null : () => _changePin(app),
                    child: const Text('UPDATE PIN'),
                  ),
                ],
              ],
            ),
          ),
          AppCard(
            title: 'Device',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                OutlinedButton(
                  onPressed: () => _forgetDevice(app),
                  child: const Text('FORGET LAST DEVICE'),
                ),
              ],
            ),
          ),
          AppCard(
            title: 'App Data',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                OutlinedButton(
                  onPressed: () => _clearAppData(app),
                  child: const Text('CLEAR APP DATA'),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

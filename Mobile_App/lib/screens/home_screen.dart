import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../widgets/app_card.dart';
import 'all_injectors_test_screen.dart';
import 'coil_test_screen.dart';
import 'injector_test_screen.dart';
import 'module_selection_screen.dart';
import 'scan_screen.dart';
import 'system_info_screen.dart';

/// Documentation/UI_UX_SPEC.md §4.2 (disconnected) / §4.5 (connected) —
/// same screen, state-driven.
class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final connected = app.isConnected;

    return Scaffold(
      appBar: AppBar(
        title: RichText(
          text: const TextSpan(
            style: TextStyle(
                fontWeight: FontWeight.w900,
                fontSize: 18,
                color: AppColors.textPrimary),
            children: [
              TextSpan(text: 'TIFF '),
              TextSpan(
                  text: 'TESTER',
                  style: TextStyle(color: AppColors.brandRed)),
              TextSpan(text: ' PRO'),
            ],
          ),
        ),
        actions: [
          Icon(
            connected ? Icons.bluetooth_connected : Icons.bluetooth,
            color: connected ? AppColors.success : AppColors.textSecondary,
          ),
          const SizedBox(width: 16),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          AppCard(
            title: 'Connection',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                StatusRow(
                  label: 'Status',
                  value: connected ? 'CONNECTED' : 'DISCONNECTED',
                  valueColor: connected ? AppColors.success : AppColors.danger,
                ),
                StatusRow(
                  label: 'Device',
                  value: app.connectedDevice?.platformName.isNotEmpty == true
                      ? app.connectedDevice!.platformName
                      : (connected ? app.connectedDevice!.remoteId.str : 'Not Connected'),
                ),
                const SizedBox(height: 12),
                ElevatedButton(
                  onPressed: () {
                    if (connected) {
                      app.disconnect();
                    } else {
                      Navigator.of(context).push(
                        MaterialPageRoute(builder: (_) => const ScanScreen()),
                      );
                    }
                  },
                  child: Text(connected ? 'DISCONNECT' : 'SCAN FOR DEVICES'),
                ),
              ],
            ),
          ),
          AppCard(
            title: 'Module',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                StatusRow(
                  label: 'Active profile',
                  value: app.activeModuleId ?? 'None selected',
                ),
                const SizedBox(height: 12),
                OutlinedButton(
                  onPressed: connected
                      ? () => Navigator.of(context).push(MaterialPageRoute(
                          builder: (_) => const ModuleSelectionScreen()))
                      : null,
                  child: const Text('SELECT MODULE'),
                ),
              ],
            ),
          ),
          AppCard(
            title: 'ECU Control',
            child: Row(
              children: [
                Expanded(
                  child: ElevatedButton(
                    style: ElevatedButton.styleFrom(
                        backgroundColor: AppColors.success),
                    onPressed: connected ? () => app.powerOn() : null,
                    child: const Text('ECU ON'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: ElevatedButton(
                    onPressed: connected ? () => app.powerOff() : null,
                    child: const Text('ECU OFF'),
                  ),
                ),
              ],
            ),
          ),
          AppCard(
            title: 'Quick Tests',
            child: GridView.count(
              crossAxisCount: 2,
              shrinkWrap: true,
              physics: const NeverScrollableScrollPhysics(),
              mainAxisSpacing: 12,
              crossAxisSpacing: 12,
              childAspectRatio: 1.5,
              children: [
                _QuickTestTile(
                  icon: Icons.bolt,
                  iconColor: Colors.lightBlueAccent,
                  label: 'Injectors',
                  sublabel: '1–4',
                  enabled: connected,
                  onTap: () => Navigator.of(context).push(MaterialPageRoute(
                      builder: (_) => const InjectorTestScreen())),
                ),
                _QuickTestTile(
                  icon: Icons.flash_on,
                  iconColor: Colors.amber,
                  label: 'Coils',
                  sublabel: '1–4',
                  enabled: connected,
                  onTap: () => Navigator.of(context).push(
                      MaterialPageRoute(builder: (_) => const CoilTestScreen())),
                ),
                _QuickTestTile(
                  icon: Icons.groups,
                  iconColor: AppColors.purple,
                  label: 'All Injectors',
                  sublabel: 'Test all',
                  enabled: connected,
                  onTap: () => Navigator.of(context).push(MaterialPageRoute(
                      builder: (_) => const AllInjectorsTestScreen())),
                ),
                _QuickTestTile(
                  icon: Icons.info_outline,
                  iconColor: Colors.tealAccent,
                  label: 'System Info',
                  sublabel: 'View info',
                  enabled: true,
                  onTap: () => Navigator.of(context).push(MaterialPageRoute(
                      builder: (_) => const SystemInfoScreen())),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _QuickTestTile extends StatelessWidget {
  final IconData icon;
  final Color iconColor;
  final String label;
  final String sublabel;
  final bool enabled;
  final VoidCallback onTap;

  const _QuickTestTile({
    required this.icon,
    required this.iconColor,
    required this.label,
    required this.sublabel,
    required this.enabled,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Opacity(
      opacity: enabled ? 1.0 : 0.4,
      child: InkWell(
        borderRadius: BorderRadius.circular(12),
        onTap: enabled
            ? onTap
            : () {
                ScaffoldMessenger.of(context).showSnackBar(const SnackBar(
                    content: Text('Connect to a device first')));
              },
        child: Container(
          decoration: BoxDecoration(
            color: AppColors.background,
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: AppColors.surfaceBorder),
          ),
          padding: const EdgeInsets.all(12),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(icon, color: iconColor, size: 28),
              const SizedBox(height: 8),
              Text(label,
                  style: const TextStyle(
                      fontWeight: FontWeight.bold, color: AppColors.textPrimary)),
              Text(sublabel,
                  style: const TextStyle(
                      color: AppColors.textSecondary, fontSize: 12)),
            ],
          ),
        ),
      ),
    );
  }
}

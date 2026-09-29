import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../widgets/app_card.dart';
import 'all_injectors_test_screen.dart';
import 'coil_test_screen.dart';
import 'injector_test_screen.dart';

/// Fills the "Tests" bottom-nav tab, which the reference screenshots don't
/// show a design for — flagged as a gap in Documentation/UI_UX_SPEC.md §6
/// item 3. Resolved here as a fuller test menu: the per-channel screens
/// already reachable from Home, plus the sensor-based quick tests
/// (resistance/short-to-ground/current-monitor) that only exist here,
/// since Home's grid has no slot for them.
class TestsScreen extends StatelessWidget {
  const TestsScreen({super.key});

  Future<void> _runQuickTest(BuildContext context, String test) async {
    final app = context.read<AppState>();
    if (!app.isConnected) {
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('Connect to a device first')));
      return;
    }
    try {
      await app.runQuickTest(test);
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('$test requested — see Results tab.')));
      }
    } on StateError catch (e) {
      if (context.mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(e.message)));
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();

    return Scaffold(
      appBar: AppBar(title: const Text('TESTS')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          AppCard(
            title: 'Channel Tests',
            child: Column(
              children: [
                _testLink(context, 'Injector Test', Icons.bolt,
                    () => Navigator.of(context).push(MaterialPageRoute(
                        builder: (_) => const InjectorTestScreen()))),
                _testLink(context, 'Ignition Coil Test', Icons.flash_on,
                    () => Navigator.of(context).push(MaterialPageRoute(
                        builder: (_) => const CoilTestScreen()))),
                _testLink(context, 'All Injectors Test', Icons.groups,
                    () => Navigator.of(context).push(MaterialPageRoute(
                        builder: (_) => const AllInjectorsTestScreen()))),
              ],
            ),
          ),
          AppCard(
            title: 'Sensor Quick Tests',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Text(
                  'Computed from the Nano\'s existing voltage/current '
                  'sensors — real results today, unlike the channel tests '
                  'above which need driver hardware that doesn\'t exist yet.',
                  style: TextStyle(color: AppColors.textSecondary, fontSize: 12),
                ),
                const SizedBox(height: 12),
                ElevatedButton(
                  onPressed: app.isConnected
                      ? () => _runQuickTest(context, 'resistance')
                      : null,
                  child: const Text('RUN RESISTANCE TEST'),
                ),
                const SizedBox(height: 8),
                ElevatedButton(
                  onPressed: app.isConnected
                      ? () => _runQuickTest(context, 'short_to_ground')
                      : null,
                  child: const Text('RUN SHORT-TO-GROUND TEST'),
                ),
                const SizedBox(height: 8),
                ElevatedButton(
                  onPressed: app.isConnected
                      ? () => _runQuickTest(context, 'current_monitor')
                      : null,
                  child: const Text('RUN CURRENT MONITOR TEST'),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _testLink(
      BuildContext context, String label, IconData icon, VoidCallback onTap) {
    return ListTile(
      contentPadding: EdgeInsets.zero,
      leading: Icon(icon, color: AppColors.brandRed),
      title: Text(label),
      trailing: const Icon(Icons.chevron_right, color: AppColors.textSecondary),
      onTap: onTap,
    );
  }
}

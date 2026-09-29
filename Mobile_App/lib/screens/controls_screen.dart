import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../widgets/app_card.dart';

/// Manual output control screen (Documentation/PRD.md §11, vision brief
/// §17 "Control Screen" / §18 "Safety Control" / §37 "Emergency ALL
/// OUTPUTS OFF"). Distinguishes the one relay that physically exists — the
/// critical DUT relay, driven by the Nano independently of the ESP32 — from
/// the auxiliary relay/MOSFET outputs the vision brief describes, which
/// need Phase 1 driver hardware that doesn't exist yet
/// (Documentation/ROADMAP.md). Per brief §17: "Use clear labels rather than
/// confusing relay numbers" and never conflate the DUT relay with an
/// auxiliary one.
class ControlsScreen extends StatelessWidget {
  const ControlsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final connected = app.isConnected;
    final status = app.nanoStatus;

    return Scaffold(
      appBar: AppBar(title: const Text('CONTROLS')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          AppCard(
            title: 'DUT Relay',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                StatusRow(
                  label: 'State',
                  value: status.relay ? 'ENERGIZED' : 'OFF',
                  valueColor:
                      status.relay ? AppColors.success : AppColors.textSecondary,
                ),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: ElevatedButton(
                        style: ElevatedButton.styleFrom(
                            backgroundColor: AppColors.success),
                        onPressed: connected ? () => app.powerOn() : null,
                        child: const Text('DUT ON'),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: ElevatedButton(
                        onPressed: connected ? () => app.powerOff() : null,
                        child: const Text('DUT OFF'),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                OutlinedButton(
                  onPressed: connected ? () => app.resetFault() : null,
                  child: const Text('RESET FAULT'),
                ),
              ],
            ),
          ),
          AppCard(
            title: 'Auxiliary Relays',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Text(
                  'Separate from the DUT relay above — these drive other '
                  'bench outputs, not the device under test.',
                  style: TextStyle(color: AppColors.textSecondary, fontSize: 12),
                ),
                const SizedBox(height: 12),
                _PendingHardwareRow(labels: const ['RELAY 2', 'RELAY 3', 'RELAY 4']),
              ],
            ),
          ),
          AppCard(
            title: 'MOSFET Outputs',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                _PendingHardwareRow(labels: const ['MOSFET 1', 'MOSFET 2']),
              ],
            ),
          ),
          const SizedBox(height: 8),
          SizedBox(
            width: double.infinity,
            child: ElevatedButton.icon(
              style: ElevatedButton.styleFrom(
                backgroundColor: AppColors.danger,
                padding: const EdgeInsets.symmetric(vertical: 18),
              ),
              icon: const Icon(Icons.power_settings_new),
              label: const Text('ALL OUTPUTS OFF',
                  style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
              onPressed: () => _allOutputsOff(context, app),
            ),
          ),
          const SizedBox(height: 8),
          const Center(
            child: Text(
              'Immediately cuts the DUT relay and aborts any running test. '
              'No confirmation — this is a safety function.',
              textAlign: TextAlign.center,
              style: TextStyle(color: AppColors.textSecondary, fontSize: 12),
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _allOutputsOff(BuildContext context, AppState app) async {
    if (!app.isConnected) {
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Connect to a device first')));
      return;
    }
    try {
      await app.allOutputsOff();
      if (!context.mounted) return;
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('All outputs off')));
    } catch (_) {
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Failed to reach device')));
    }
  }
}

/// A row of buttons for outputs that need Phase 1 driver hardware
/// (Documentation/ROADMAP.md) which doesn't exist on this bench yet — shown
/// disabled rather than wired to a command the firmware would silently
/// drop, matching this codebase's "don't fake it" convention (compare the
/// channel-test screens' honest `NOT_IMPLEMENTED` results).
class _PendingHardwareRow extends StatelessWidget {
  final List<String> labels;
  const _PendingHardwareRow({required this.labels});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        for (final label in labels) ...[
          Expanded(
            child: Opacity(
              opacity: 0.4,
              child: OutlinedButton(
                onPressed: () => ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(
                    content: Text(
                        'Needs driver hardware not yet installed (Roadmap Phase 1)'),
                  ),
                ),
                child: Text(label),
              ),
            ),
          ),
          if (label != labels.last) const SizedBox(width: 8),
        ],
      ],
    );
  }
}

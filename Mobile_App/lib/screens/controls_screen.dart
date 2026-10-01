import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../widgets/app_card.dart';
import '../widgets/live_data_card.dart';

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
          const LiveDataCard(),
          AppCard(
            title: 'DUT Relay (Relay 1)',
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
                Row(
                  children: [
                    Expanded(
                      child: OutlinedButton(
                        onPressed: connected ? () => app.resetFault() : null,
                        child: const Text('RESET FAULT'),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: OutlinedButton(
                        onPressed:
                            connected ? () => _testRelay(context, app, 1) : null,
                        child: const Text('TEST RELAY 1'),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                const Text(
                  'Relay test cycles the relay 3x and checks DUT voltage '
                  'follows it. The DUT rail must be connected for a PASS.',
                  style: TextStyle(color: AppColors.textSecondary, fontSize: 12),
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
                  'bench outputs, not the device under test. All turn off '
                  'on e-stop, any fault, link loss and ALL OUTPUTS OFF.',
                  style: TextStyle(color: AppColors.textSecondary, fontSize: 12),
                ),
                const SizedBox(height: 8),
                for (var n = 2; n <= 4; n++)
                  _RelayRow(
                    n: n,
                    on: status.relays[n - 1],
                    enabled: connected,
                    onSet: (on) => _setRelay(context, app, n, on),
                    onTest: () => _testRelay(context, app, n),
                  ),
                const SizedBox(height: 4),
                const Text(
                  'Relays 2-4 have no feedback sensor, so TEST reports '
                  'ACTUATED (cycles completed) — confirm the click or load '
                  'yourself.',
                  style: TextStyle(color: AppColors.textSecondary, fontSize: 12),
                ),
              ],
            ),
          ),
          AppCard(
            title: 'Relay Polarity',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                StatusRow(
                  label: 'All four relays',
                  value: status.relayActiveLow
                      ? 'ACTIVE-LOW (LOW = ON)'
                      : 'ACTIVE-HIGH (HIGH = ON)',
                ),
                const SizedBox(height: 8),
                OutlinedButton(
                  onPressed: connected
                      ? () => _confirmPolarity(context, app, !status.relayActiveLow)
                      : null,
                  child: Text(status.relayActiveLow
                      ? 'SWITCH TO ACTIVE-HIGH'
                      : 'SWITCH TO ACTIVE-LOW'),
                ),
                const SizedBox(height: 8),
                const Text(
                  'Must match your relay module. All relays are forced off '
                  'before and after the change. Active-low is the firmware '
                  'default and the safer choice for a pulled-up module.',
                  style: TextStyle(color: AppColors.textSecondary, fontSize: 12),
                ),
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

  Future<void> _setRelay(
      BuildContext context, AppState app, int n, bool on) async {
    await _guard(context, () => app.setRelay(n, on));
  }

  Future<void> _confirmPolarity(
      BuildContext context, AppState app, bool activeLow) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Change relay polarity?'),
        content: Text(
            'Set all relays to ${activeLow ? 'ACTIVE-LOW' : 'ACTIVE-HIGH'}. '
            'A wrong setting makes relays energize when they should be off. '
            'Confirm it matches your relay module.'),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: const Text('CANCEL')),
          TextButton(
              onPressed: () => Navigator.pop(context, true),
              child: const Text('CHANGE')),
        ],
      ),
    );
    if (ok == true && context.mounted) {
      await _guard(context, () => app.setRelayPolarity(activeLow: activeLow));
    }
  }

  Future<void> _testRelay(BuildContext context, AppState app, int n) async {
    await _guard(context, () async {
      await app.testRelay(n);
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(
            content: Text('Testing relay $n — result will appear in Results')));
      }
    });
  }

  /// Runs a device command, surfacing failures (including "not supported by
  /// the V2 firmware") instead of silently swallowing them.
  Future<void> _guard(BuildContext context, Future<void> Function() op) async {
    try {
      await op();
    } catch (e) {
      if (!context.mounted) return;
      final msg = e is StateError ? e.message : 'Failed to reach device';
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
    }
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

/// One auxiliary relay: label, live state, ON/OFF switch and a TEST button.
class _RelayRow extends StatelessWidget {
  final int n;
  final bool on;
  final bool enabled;
  final ValueChanged<bool> onSet;
  final VoidCallback onTest;

  const _RelayRow({
    required this.n,
    required this.on,
    required this.enabled,
    required this.onSet,
    required this.onTest,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('RELAY $n',
                  style: const TextStyle(fontWeight: FontWeight.bold)),
              Text(on ? 'ENERGIZED' : 'OFF',
                  style: TextStyle(
                      fontSize: 12,
                      color: on ? AppColors.success : AppColors.textSecondary)),
            ],
          ),
        ),
        Switch(value: on, onChanged: enabled ? onSet : null),
        const SizedBox(width: 8),
        OutlinedButton(
          onPressed: enabled ? onTest : null,
          child: const Text('TEST'),
        ),
      ],
    );
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

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../widgets/app_card.dart';

/// Documentation/UI_UX_SPEC.md §4.11 — explicitly flagged there as having
/// no reference design; this is a reasonable first pass, not a pixel-exact
/// match of anything supplied.
class SystemInfoScreen extends StatelessWidget {
  const SystemInfoScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final s = app.nanoStatus;

    return Scaffold(
      appBar: AppBar(title: const Text('SYSTEM INFO')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          AppCard(
            title: 'App',
            child: const Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                StatusRow(label: 'App version', value: '0.2.0'),
                StatusRow(label: 'Platform', value: 'Android'),
              ],
            ),
          ),
          AppCard(
            title: 'Connected Device',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                StatusRow(
                  label: 'Device',
                  value: app.isConnected
                      ? (app.connectedDevice?.platformName ?? 'Unknown')
                      : 'Not connected',
                ),
                StatusRow(
                  label: 'Address',
                  value: app.connectedDevice?.remoteId.str ?? '—',
                ),
              ],
            ),
          ),
          AppCard(
            title: 'Live Bench Status',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                StatusRow(
                    label: 'Supply voltage', value: '${s.supplyV.toStringAsFixed(2)} V'),
                StatusRow(
                    label: 'DUT voltage', value: '${s.dutV.toStringAsFixed(2)} V'),
                StatusRow(
                    label: 'Current', value: '${s.currentA.toStringAsFixed(2)} A'),
                StatusRow(label: 'Relay', value: s.relay ? 'ON' : 'OFF'),
                StatusRow(label: 'Fault', value: s.fault ? s.faultText : 'None'),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../screens/live_graph_screen.dart';
import 'app_card.dart';

/// Live data preview: supply/DUT voltage, DUT current, position,
/// temperature and subsystem readiness, in the layout of the bench display:
///
///     Supply voltage - 12.0 V
///     DUT voltage - 11.8 V
///     ...
///     CAN - READY
///
/// Values read "--" until a device is connected, rather than showing zeros
/// that look like real measurements.
class LiveDataCard extends StatelessWidget {
  const LiveDataCard({super.key});

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final connected = app.isConnected;
    final s = app.nanoStatus;

    String val(double v, int digits, String unit, {bool reported = true}) {
      if (!connected) return '-- $unit';
      if (!reported) return 'n/a';
      return '${v.toStringAsFixed(digits)} $unit';
    }

    final String system;
    final Color systemColor;
    if (!connected) {
      system = 'OFFLINE';
      systemColor = AppColors.textSecondary;
    } else if (s.fault) {
      system = s.faultText.isEmpty ? 'FAULT' : 'FAULT: ${s.faultText}';
      systemColor = AppColors.danger;
    } else if (s.systemReady) {
      system = 'READY';
      systemColor = AppColors.success;
    } else {
      system = 'NOT READY';
      systemColor = AppColors.warning;
    }

    // selfTest: -1 not run, 0 failed, 1 passed. A failed self-test overrides
    // READY, because the flag alone only means "initialised".
    Widget ready(String label, bool ok,
        {String bad = 'NOT READY', int selfTest = -1}) {
      String value;
      Color color;
      if (!connected) {
        value = '--';
        color = AppColors.textSecondary;
      } else if (!ok) {
        value = bad;
        color = AppColors.warning;
      } else if (selfTest == 0) {
        value = 'SELF-TEST FAILED';
        color = AppColors.danger;
      } else if (selfTest == 1) {
        value = 'READY ✓';
        color = AppColors.success;
      } else {
        value = 'READY';
        color = AppColors.success;
      }
      return StatusRow(label: label, value: value, valueColor: color);
    }

    return AppCard(
      title: 'Live Data',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          StatusRow(label: 'Supply voltage', value: val(s.supplyV, 1, 'V')),
          StatusRow(label: 'DUT voltage', value: val(s.dutV, 1, 'V')),
          StatusRow(label: 'DUT current', value: val(s.currentA, 2, 'A')),
          StatusRow(label: 'Position', value: val(s.positionPct, 0, '%')),
          // V2 reports a raw ADC voltage here, not degrees C.
          StatusRow(
              label: 'Temperature',
              value: val(s.tempC, 1, '°C', reported: s.tempReported)),
          const Divider(height: 24),
          ready('CAN', s.canReady, selfTest: s.canSelfTest),
          ready('K-LINE', s.klineReady, selfTest: s.klineSelfTest),
          ready('INA219', s.inaReady, bad: 'NOT FOUND'),
          StatusRow(
              label: 'SYSTEM', value: system, valueColor: systemColor),
          const SizedBox(height: 8),
          OutlinedButton.icon(
            icon: const Icon(Icons.show_chart),
            label: const Text('VIEW GRAPH'),
            onPressed: () => Navigator.of(context).push(
                MaterialPageRoute(builder: (_) => const LiveGraphScreen())),
          ),
        ],
      ),
    );
  }
}

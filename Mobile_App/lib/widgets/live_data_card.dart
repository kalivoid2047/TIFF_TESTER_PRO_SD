import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../theme/app_theme.dart';
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
    // The V2 Bluetooth Classic firmware reports only voltage/current/relay,
    // so position, temperature and the readiness flags are "n/a", not zero
    // or NOT READY.
    final classic = connected && app.isClassic;
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

    Widget ready(String label, bool ok, {String bad = 'NOT READY'}) =>
        StatusRow(
          label: label,
          value: !connected ? '--' : (classic ? 'n/a' : (ok ? 'READY' : bad)),
          valueColor: (!connected || classic)
              ? AppColors.textSecondary
              : (ok ? AppColors.success : AppColors.warning),
        );

    return AppCard(
      title: 'Live Data',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          StatusRow(label: 'Supply voltage', value: val(s.supplyV, 1, 'V')),
          StatusRow(label: 'DUT voltage', value: val(s.dutV, 1, 'V')),
          StatusRow(label: 'DUT current', value: val(s.currentA, 2, 'A')),
          StatusRow(
              label: 'Position',
              value: val(s.positionPct, 0, '%', reported: !classic)),
          StatusRow(
              label: 'Temperature',
              value: val(s.tempC, 1, '°C', reported: !classic)),
          const Divider(height: 24),
          ready('CAN', s.canReady),
          ready('K-LINE', s.klineReady),
          ready('INA219', s.inaReady, bad: 'NOT FOUND'),
          StatusRow(
              label: 'SYSTEM', value: system, valueColor: systemColor),
        ],
      ),
    );
  }
}

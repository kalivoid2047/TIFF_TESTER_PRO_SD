import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';

import '../models/live_sample.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../widgets/app_card.dart';

/// Scrolling graph of the live bench data (supply/DUT voltage, current,
/// position, temperature) over the current session, with CSV copy. The data
/// is the same status stream the Live Data card shows, kept in memory by
/// `AppState.liveHistory` (not the tester's SD log).
class LiveGraphScreen extends StatelessWidget {
  const LiveGraphScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final samples = app.liveHistory;
    final hasTemp = samples.any((s) => !s.tempC.isNaN);

    return Scaffold(
      appBar: AppBar(
        title: const Text('LIVE GRAPH'),
        actions: [
          IconButton(
            tooltip: 'Copy CSV',
            icon: const Icon(Icons.copy),
            onPressed: samples.isEmpty
                ? null
                : () async {
                    await Clipboard.setData(
                        ClipboardData(text: LiveSample.toCsv(samples)));
                    if (!context.mounted) return;
                    ScaffoldMessenger.of(context).showSnackBar(SnackBar(
                        content:
                            Text('Copied ${samples.length} samples as CSV')));
                  },
          ),
          IconButton(
            tooltip: 'Clear',
            icon: const Icon(Icons.delete_outline),
            onPressed: samples.isEmpty ? null : app.clearLiveHistory,
          ),
        ],
      ),
      body: samples.length < 2
          ? const Center(
              child: Padding(
                padding: EdgeInsets.all(24),
                child: Text(
                  'Waiting for live data — connect to a device and the graph '
                  'fills as status updates arrive.',
                  textAlign: TextAlign.center,
                  style: TextStyle(color: AppColors.textSecondary),
                ),
              ),
            )
          : ListView(
              padding: const EdgeInsets.all(16),
              children: [
                _chart('Voltage (V)', samples, [
                  _Series('Supply', (s) => s.supplyV, AppColors.warning),
                  _Series('DUT', (s) => s.dutV, AppColors.success),
                ]),
                _chart('DUT current (A)', samples, [
                  _Series('Current', (s) => s.currentA, Colors.lightBlueAccent)
                ]),
                _chart('DUT power (W)', samples,
                    [_Series('Power', (s) => s.powerW, AppColors.purple)]),
                _chart(
                    'Position (%)',
                    samples,
                    [
                      _Series(
                          'Position', (s) => s.positionPct, Colors.tealAccent)
                    ],
                    fixedRange: const [0, 100]),
                if (hasTemp)
                  _chart('Temperature (°C)', samples,
                      [_Series('Temp', (s) => s.tempC, AppColors.danger)]),
                Text(
                  '${samples.length} samples · newest on the right. Kept in '
                  'the app for this session only '
                  '(last ${AppState.maxLiveSamples}).',
                  style: const TextStyle(
                      color: AppColors.textSecondary, fontSize: 12),
                ),
              ],
            ),
    );
  }

  Widget _chart(String title, List<LiveSample> samples, List<_Series> series,
      {List<double>? fixedRange}) {
    return AppCard(
      title: title,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          SizedBox(
            height: 140,
            child: CustomPaint(
              painter: _ChartPainter(
                values: [
                  for (final s in series) samples.map(s.read).toList(),
                ],
                colors: [for (final s in series) s.color],
                fixedRange: fixedRange,
              ),
            ),
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 16,
            children: [
              for (final s in series)
                Row(mainAxisSize: MainAxisSize.min, children: [
                  Container(width: 10, height: 10, color: s.color),
                  const SizedBox(width: 6),
                  Text(
                    '${s.name}: ${_latest(samples, s)}',
                    style: const TextStyle(fontSize: 12),
                  ),
                ]),
            ],
          ),
        ],
      ),
    );
  }

  String _latest(List<LiveSample> samples, _Series s) {
    for (var i = samples.length - 1; i >= 0; i--) {
      final v = s.read(samples[i]);
      if (!v.isNaN) return v.toStringAsFixed(2);
    }
    return '--';
  }
}

class _Series {
  final String name;
  final double Function(LiveSample) read;
  final Color color;
  const _Series(this.name, this.read, this.color);
}

class _ChartPainter extends CustomPainter {
  final List<List<double>> values;
  final List<Color> colors;
  final List<double>? fixedRange;

  _ChartPainter({required this.values, required this.colors, this.fixedRange});

  @override
  void paint(Canvas canvas, Size size) {
    final all = [
      for (final v in values) ...v.where((x) => !x.isNaN),
    ];
    if (all.isEmpty) return;

    var lo = fixedRange?[0] ?? all.reduce(math.min);
    var hi = fixedRange?[1] ?? all.reduce(math.max);
    if (fixedRange == null) {
      final pad = (hi - lo).abs() < 1e-6 ? 1.0 : (hi - lo) * 0.1;
      lo -= pad;
      hi += pad;
    }

    final grid = Paint()
      ..color = AppColors.surfaceBorder
      ..strokeWidth = 1;
    final label = TextPainter(textDirection: TextDirection.ltr);
    for (var i = 0; i <= 4; i++) {
      final y = size.height * i / 4;
      canvas.drawLine(Offset(0, y), Offset(size.width, y), grid);
      label.text = TextSpan(
        text: (hi - (hi - lo) * i / 4).toStringAsFixed(1),
        style: const TextStyle(color: AppColors.textSecondary, fontSize: 9),
      );
      label.layout();
      label.paint(canvas, Offset(2, y == 0 ? 0 : y - label.height));
    }

    for (var s = 0; s < values.length; s++) {
      final v = values[s];
      final paint = Paint()
        ..color = colors[s]
        ..strokeWidth = 2
        ..style = PaintingStyle.stroke;
      final path = Path();
      var pen = false;
      for (var i = 0; i < v.length; i++) {
        if (v[i].isNaN) {
          pen = false;
          continue;
        }
        final x = v.length == 1 ? 0.0 : size.width * i / (v.length - 1);
        final y = size.height * (1 - (v[i] - lo) / (hi - lo));
        if (!pen) {
          path.moveTo(x, y);
          pen = true;
        } else {
          path.lineTo(x, y);
        }
      }
      canvas.drawPath(path, paint);
    }
  }

  @override
  bool shouldRepaint(covariant _ChartPainter old) => true;
}

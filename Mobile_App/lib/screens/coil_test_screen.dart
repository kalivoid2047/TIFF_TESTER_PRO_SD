import 'dart:async';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/test_result.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../widgets/app_card.dart';

/// Documentation/UI_UX_SPEC.md §4.7.
class CoilTestScreen extends StatefulWidget {
  const CoilTestScreen({super.key});

  @override
  State<CoilTestScreen> createState() => _CoilTestScreenState();
}

class _CoilTestScreenState extends State<CoilTestScreen> {
  int _selectedCoil = 1;
  double _dwellMs = 3.0;
  int _durationS = 5;

  bool _testing = false;
  int _elapsedS = 0;
  Timer? _elapsedTimer;
  StreamSubscription<TestResult>? _resultSub;
  TestResult? _lastResult;
  String _liveResponse = '—';

  @override
  void dispose() {
    _elapsedTimer?.cancel();
    _resultSub?.cancel();
    super.dispose();
  }

  Future<void> _startTest() async {
    final app = context.read<AppState>();

    setState(() {
      _testing = true;
      _elapsedS = 0;
      _liveResponse = '—';
    });

    _resultSub?.cancel();
    _resultSub = app.ble.resultStream.listen((r) {
      if (!r.testType.contains('coil')) return;
      setState(() {
        _lastResult = r;
        _liveResponse = r.response;
        _testing = false;
      });
      _elapsedTimer?.cancel();
    });

    _elapsedTimer?.cancel();
    _elapsedTimer = Timer.periodic(const Duration(seconds: 1), (_) {
      setState(() => _elapsedS++);
    });

    await app.runCoilTest(
      channel: _selectedCoil,
      dwellMs: _dwellMs,
      durationS: _durationS,
    );
  }

  Future<void> _stopTest() async {
    await context.read<AppState>().stopTest();
    _elapsedTimer?.cancel();
    setState(() => _testing = false);
  }

  String get _elapsedLabel {
    final m = (_elapsedS ~/ 60).toString().padLeft(2, '0');
    final s = (_elapsedS % 60).toString().padLeft(2, '0');
    return '00:$m:$s';
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('IGNITION COIL TEST')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          AppCard(
            title: 'Select Coil',
            child: Row(
              children: List.generate(4, (i) {
                final n = i + 1;
                final selected = _selectedCoil == n;
                return Expanded(
                  child: Padding(
                    padding: EdgeInsets.only(right: n < 4 ? 8 : 0),
                    child: OutlinedButton(
                      style: OutlinedButton.styleFrom(
                        backgroundColor:
                            selected ? AppColors.brandRed : Colors.transparent,
                        foregroundColor:
                            selected ? Colors.white : AppColors.textPrimary,
                      ),
                      onPressed:
                          _testing ? null : () => setState(() => _selectedCoil = n),
                      child: Text('COIL $n'),
                    ),
                  ),
                );
              }),
            ),
          ),
          AppCard(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Text('DWELL TIME (ms)',
                    style: TextStyle(color: AppColors.textSecondary, fontSize: 12)),
                Slider(
                  value: _dwellMs,
                  min: 0.5,
                  max: 8.0,
                  divisions: 15,
                  label: _dwellMs.toStringAsFixed(1),
                  onChanged:
                      _testing ? null : (v) => setState(() => _dwellMs = v),
                ),
                const SizedBox(height: 8),
                const Text('TEST DURATION (s)',
                    style: TextStyle(color: AppColors.textSecondary, fontSize: 12)),
                Slider(
                  value: _durationS.toDouble(),
                  min: 1,
                  max: 15,
                  divisions: 14,
                  label: '$_durationS',
                  onChanged: _testing
                      ? null
                      : (v) => setState(() => _durationS = v.round()),
                ),
              ],
            ),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              backgroundColor: _testing ? AppColors.danger : AppColors.success,
            ),
            onPressed: _testing ? _stopTest : _startTest,
            child: Text(_testing ? 'STOP TEST' : 'START TEST'),
          ),
          const SizedBox(height: 8),
          AppCard(
            title: 'Coil Status',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                StatusRow(
                  label: 'Status',
                  value: _testing
                      ? 'TESTING...'
                      : (_lastResult == null ? 'IDLE' : 'DONE'),
                  valueColor: _testing ? AppColors.warning : null,
                ),
                if (_testing)
                  StatusRow(label: 'Time Elapsed', value: _elapsedLabel)
                else
                  StatusRow(
                    label: 'Last Result',
                    value: _lastResult == null
                        ? '—'
                        : (_lastResult!.pass ? 'PASS' : 'FAIL'),
                    valueColor: _lastResult == null
                        ? null
                        : (_lastResult!.pass ? AppColors.success : AppColors.danger),
                  ),
                StatusRow(label: 'Response', value: _liveResponse),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

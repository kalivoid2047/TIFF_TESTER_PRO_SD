import 'dart:async';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/test_result.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';
import '../widgets/app_card.dart';

/// Documentation/UI_UX_SPEC.md §4.8.
///
/// Firmware currently answers RUN_ALL_INJECTORS as a single NOT_IMPLEMENTED
/// result (no injector driver hardware exists yet — see
/// Documentation/ROADMAP.md Phase 1), so "progress" here is 0/4 -> 1/4
/// rather than a real per-channel sequence. The UI is built to the full
/// 4-step spec so no screen changes are needed once real hardware lands.
class AllInjectorsTestScreen extends StatefulWidget {
  const AllInjectorsTestScreen({super.key});

  @override
  State<AllInjectorsTestScreen> createState() =>
      _AllInjectorsTestScreenState();
}

class _AllInjectorsTestScreenState extends State<AllInjectorsTestScreen> {
  double _pulseWidthMs = 3.0;
  int _durationPerS = 3;

  bool _testing = false;
  int _progress = 0;
  StreamSubscription<TestResult>? _resultSub;
  String _status = 'IDLE';

  @override
  void dispose() {
    _resultSub?.cancel();
    super.dispose();
  }

  Future<void> _start() async {
    final app = context.read<AppState>();
    if (!app.isConnected) {
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Connect to a device first')));
      return;
    }

    setState(() {
      _testing = true;
      _progress = 0;
      _status = 'TESTING';
    });

    _resultSub?.cancel();
    _resultSub = app.ble.resultStream.listen((r) {
      if (!r.testType.contains('all_injectors')) return;
      setState(() {
        _progress = 4; // firmware answers as one combined result today
        _status = 'COMPLETE';
        _testing = false;
      });
    });

    try {
      await app.runAllInjectors(
        pulseWidthMs: _pulseWidthMs,
        durationPerS: _durationPerS,
      );
    } catch (_) {
      if (!mounted) return;
      setState(() {
        _testing = false;
        _status = 'IDLE';
      });
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Failed to start test: device disconnected')));
    }
  }

  Future<void> _stop() async {
    try {
      await context.read<AppState>().stopTest();
    } catch (_) {
      // Device already disconnected; fall through to local cleanup.
    }
    if (!mounted) return;
    setState(() {
      _testing = false;
      _status = 'IDLE';
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('ALL INJECTORS TEST')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          const SizedBox(height: 8),
          const Center(
            child: Icon(Icons.groups, color: AppColors.purple, size: 64),
          ),
          const SizedBox(height: 12),
          const Center(
            child: Text(
              'This will test all injectors sequentially.',
              textAlign: TextAlign.center,
              style: TextStyle(color: AppColors.textSecondary),
            ),
          ),
          const SizedBox(height: 16),
          AppCard(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Text('PULSE WIDTH (ms)',
                    style: TextStyle(color: AppColors.textSecondary, fontSize: 12)),
                Slider(
                  value: _pulseWidthMs,
                  min: 0.5,
                  max: 8.0,
                  divisions: 15,
                  label: _pulseWidthMs.toStringAsFixed(1),
                  onChanged: _testing
                      ? null
                      : (v) => setState(() => _pulseWidthMs = v),
                ),
                const SizedBox(height: 8),
                const Text('TEST DURATION PER INJECTOR (s)',
                    style: TextStyle(color: AppColors.textSecondary, fontSize: 12)),
                Slider(
                  value: _durationPerS.toDouble(),
                  min: 1,
                  max: 10,
                  divisions: 9,
                  label: '$_durationPerS',
                  onChanged: _testing
                      ? null
                      : (v) => setState(() => _durationPerS = v.round()),
                ),
              ],
            ),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              backgroundColor: _testing ? AppColors.danger : AppColors.purple,
            ),
            onPressed: _testing ? _stop : _start,
            child: Text(_testing ? 'STOP TEST' : 'START ALL TEST'),
          ),
          const SizedBox(height: 8),
          AppCard(
            title: 'Progress',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                StatusRow(label: 'Status', value: _status),
                StatusRow(label: 'Progress', value: '$_progress/4'),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

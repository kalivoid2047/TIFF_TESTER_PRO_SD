import 'package:flutter/material.dart';
import 'package:printing/printing.dart';
import 'package:provider/provider.dart';

import '../services/report_pdf.dart';
import '../state/app_state.dart';
import '../theme/app_theme.dart';

/// Documentation/UI_UX_SPEC.md §4.10.
class ResultsScreen extends StatelessWidget {
  const ResultsScreen({super.key});

  Future<void> _confirmClear(BuildContext context, AppState app) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AppColors.surface,
        title: const Text('Clear results?'),
        content: const Text(
            'This clears the current session\'s results. Save a PDF report first if you want to keep them.'),
        actions: [
          TextButton(
              onPressed: () => Navigator.of(ctx).pop(false),
              child: const Text('Cancel')),
          TextButton(
              onPressed: () => Navigator.of(ctx).pop(true),
              child: const Text('Clear',
                  style: TextStyle(color: AppColors.danger))),
        ],
      ),
    );
    if (confirmed == true) app.clearResults();
  }

  Future<void> _savePdf(BuildContext context, AppState app) async {
    final doc = await buildReportPdf(
      results: app.results,
      deviceName: app.connectedName,
      liveSamples: app.liveHistory,
    );
    await Printing.sharePdf(
      bytes: await doc.save(),
      filename:
          'tiff_tester_report_${DateTime.now().millisecondsSinceEpoch}.pdf',
    );
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();

    return Scaffold(
      appBar: AppBar(title: const Text('TEST RESULTS')),
      body: Column(
        children: [
          Expanded(
            child: app.results.isEmpty
                ? const Center(
                    child: Text('No test results yet — run a test to see results here.',
                        textAlign: TextAlign.center,
                        style: TextStyle(color: AppColors.textSecondary)))
                : ListView.separated(
                    padding: const EdgeInsets.all(16),
                    itemCount: app.results.length,
                    separatorBuilder: (_, _) => const Divider(
                        color: AppColors.surfaceBorder, height: 1),
                    itemBuilder: (context, i) {
                      final r = app.results[i];
                      return Padding(
                        padding: const EdgeInsets.symmetric(vertical: 10),
                        child: Row(
                          mainAxisAlignment: MainAxisAlignment.spaceBetween,
                          children: [
                            Text(r.displayName,
                                style:
                                    const TextStyle(fontWeight: FontWeight.bold)),
                            Column(
                              crossAxisAlignment: CrossAxisAlignment.end,
                              children: [
                                Text(
                                  r.pass ? 'Pass' : 'Fail',
                                  style: TextStyle(
                                    fontWeight: FontWeight.bold,
                                    color: r.pass
                                        ? AppColors.success
                                        : AppColors.danger,
                                  ),
                                ),
                                Text(
                                  '${r.timestamp.hour.toString().padLeft(2, '0')}:'
                                  '${r.timestamp.minute.toString().padLeft(2, '0')}:'
                                  '${r.timestamp.second.toString().padLeft(2, '0')}',
                                  style: const TextStyle(
                                      color: AppColors.textSecondary,
                                      fontSize: 12),
                                ),
                              ],
                            ),
                          ],
                        ),
                      );
                    },
                  ),
          ),
          Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                ElevatedButton(
                  onPressed: app.results.isEmpty
                      ? null
                      : () => _confirmClear(context, app),
                  child: const Text('CLEAR RESULTS'),
                ),
                const SizedBox(height: 8),
                ElevatedButton.icon(
                  style: ElevatedButton.styleFrom(
                      backgroundColor: const Color(0xFF7A1418)),
                  onPressed:
                      app.results.isEmpty ? null : () => _savePdf(context, app),
                  icon: const Icon(Icons.description_outlined),
                  label: const Text('SAVE PDF REPORT'),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

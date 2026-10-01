import 'package:pdf/pdf.dart';
import 'package:pdf/widgets.dart' as pw;

import '../models/live_sample.dart';
import '../models/test_result.dart';

/// Builds the PDF report described in Documentation/SRS.md §6.3: a header
/// plus one row per test result. Kept intentionally simple (no logo/custom
/// fonts) so it renders identically on every platform without bundling
/// assets.
Future<pw.Document> buildReportPdf({
  required List<TestResult> results,
  String appVersion = '0.2.0',
  String? deviceName,
  List<LiveSample> liveSamples = const [],
}) async {
  final doc = pw.Document();
  final generatedAt = DateTime.now();

  doc.addPage(
    pw.MultiPage(
      pageFormat: PdfPageFormat.a4,
      build: (context) => [
        pw.Header(
          level: 0,
          child: pw.Text('TIFF TESTER PRO — Test Report',
              style: pw.TextStyle(fontSize: 20, fontWeight: pw.FontWeight.bold)),
        ),
        pw.Text('App version: $appVersion'),
        if (deviceName != null) pw.Text('Device: $deviceName'),
        pw.Text('Generated: ${generatedAt.toIso8601String()}'),
        pw.SizedBox(height: 16),
        pw.Text('Note: device has no RTC yet — each result\'s timestamp is '
            'when the app received it, not a device-side clock.',
            style:
                pw.TextStyle(fontSize: 9, fontStyle: pw.FontStyle.italic)),
        pw.SizedBox(height: 12),
        if (results.isEmpty)
          pw.Text('No test results in this session.')
        else
          pw.TableHelper.fromTextArray(
            headers: ['Test', 'Result', 'Timestamp', 'Response'],
            data: results
                .map((r) => [
                      r.displayName,
                      r.pass ? 'PASS' : 'FAIL',
                      r.timestamp.toIso8601String(),
                      r.response,
                    ])
                .toList(),
            cellStyle: const pw.TextStyle(fontSize: 9),
            headerStyle:
                pw.TextStyle(fontSize: 9, fontWeight: pw.FontWeight.bold),
            cellAlignment: pw.Alignment.centerLeft,
          ),
        if (liveSamples.isNotEmpty) ..._liveSection(liveSamples),
        pw.SizedBox(height: 16),
        pw.Text(
          'Summary: ${results.where((r) => r.pass).length} passed, '
          '${results.where((r) => !r.pass).length} failed, '
          '${results.length} total.',
          style: pw.TextStyle(fontWeight: pw.FontWeight.bold),
        ),
      ],
    ),
  );

  return doc;
}

/// "Live data" section: min/average/max of each reading across the captured
/// session, so the report shows electrical behaviour next to the test results.
/// Temperature is omitted when the firmware doesn't report real degrees C.
List<pw.Widget> _liveSection(List<LiveSample> samples) {
  List<String> row(String name, double Function(LiveSample) f, int digits) {
    final v = samples.map(f).where((x) => !x.isNaN).toList();
    if (v.isEmpty) return [name, '--', '--', '--'];
    final min = v.reduce((a, b) => a < b ? a : b);
    final max = v.reduce((a, b) => a > b ? a : b);
    final avg = v.reduce((a, b) => a + b) / v.length;
    return [
      name,
      min.toStringAsFixed(digits),
      avg.toStringAsFixed(digits),
      max.toStringAsFixed(digits),
    ];
  }

  final span = samples.last.time.difference(samples.first.time);
  final hasTemp = samples.any((s) => !s.tempC.isNaN);

  return [
    pw.SizedBox(height: 16),
    pw.Text('Live data (this session)',
        style: pw.TextStyle(fontSize: 14, fontWeight: pw.FontWeight.bold)),
    pw.Text(
        '${samples.length} samples over ${span.inSeconds} s '
        '(${samples.first.time.toIso8601String()} to '
        '${samples.last.time.toIso8601String()}).',
        style: const pw.TextStyle(fontSize: 9)),
    pw.SizedBox(height: 6),
    pw.TableHelper.fromTextArray(
      headers: ['Reading', 'Min', 'Average', 'Max'],
      data: [
        row('Supply voltage (V)', (s) => s.supplyV, 2),
        row('DUT voltage (V)', (s) => s.dutV, 2),
        row('DUT current (A)', (s) => s.currentA, 2),
        row('DUT power (W)', (s) => s.powerW, 2),
        row('Position (%)', (s) => s.positionPct, 0),
        if (hasTemp) row('Temperature (°C)', (s) => s.tempC, 1),
      ],
      cellStyle: const pw.TextStyle(fontSize: 9),
      headerStyle: pw.TextStyle(fontSize: 9, fontWeight: pw.FontWeight.bold),
      cellAlignment: pw.Alignment.centerLeft,
    ),
  ];
}

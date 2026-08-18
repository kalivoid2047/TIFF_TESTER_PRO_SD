import 'package:pdf/pdf.dart';
import 'package:pdf/widgets.dart' as pw;

import '../models/test_result.dart';

/// Builds the PDF report described in Documentation/SRS.md §6.3: a header
/// plus one row per test result. Kept intentionally simple (no logo/custom
/// fonts) so it renders identically on every platform without bundling
/// assets.
Future<pw.Document> buildReportPdf({
  required List<TestResult> results,
  String appVersion = '0.2.0',
  String? deviceName,
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

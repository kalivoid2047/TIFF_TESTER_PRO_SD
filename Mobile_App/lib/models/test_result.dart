/// One test result, as shown on the Results screen (Documentation/
/// UI_UX_SPEC.md §4.10). Parsed from the Result characteristic's
/// "testType,PASS|FAIL,response" text (Documentation/API_PROTOCOL_SPEC.md
/// §1.3) plus a locally-captured timestamp — the board has no RTC yet, so
/// "when" is whenever the app received the notification, not a
/// device-side timestamp.
class TestResult {
  final String testType;
  final bool pass;
  final String response;
  final DateTime timestamp;

  const TestResult({
    required this.testType,
    required this.pass,
    required this.response,
    required this.timestamp,
  });

  /// Parses "testType,PASS|FAIL,response" — response may itself contain
  /// commas, so only the first two commas are treated as delimiters.
  factory TestResult.parse(String raw) {
    final firstComma = raw.indexOf(',');
    final secondComma =
        firstComma < 0 ? -1 : raw.indexOf(',', firstComma + 1);

    if (firstComma < 0 || secondComma < 0) {
      return TestResult(
        testType: raw,
        pass: false,
        response: 'MALFORMED_RESULT',
        timestamp: DateTime.now(),
      );
    }

    final type = raw.substring(0, firstComma);
    final passStr = raw.substring(firstComma + 1, secondComma);
    final response = raw.substring(secondComma + 1);

    return TestResult(
      testType: type,
      pass: passStr == 'PASS',
      response: response,
      timestamp: DateTime.now(),
    );
  }

  /// Serializes for local persistence (SharedPreferences JSON blob) —
  /// distinct from the wire format `parse()` reads, which never round-trips
  /// a timestamp.
  Map<String, dynamic> toJson() => {
        'testType': testType,
        'pass': pass,
        'response': response,
        'timestamp': timestamp.toIso8601String(),
      };

  factory TestResult.fromJson(Map<String, dynamic> json) => TestResult(
        testType: json['testType'] as String,
        pass: json['pass'] as bool,
        response: json['response'] as String,
        timestamp: DateTime.parse(json['timestamp'] as String),
      );

  /// Human-friendly label for the Results list, e.g. "resistance" ->
  /// "Resistance Test".
  String get displayName {
    final cleaned = testType.replaceAll('_', ' ').replaceAll(':', ' ');
    final words = cleaned.split(' ').where((w) => w.isNotEmpty);
    final titled = words
        .map((w) => w[0].toUpperCase() + w.substring(1))
        .join(' ');
    return '$titled Test';
  }
}

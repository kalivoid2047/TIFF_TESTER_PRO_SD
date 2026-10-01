/// One line of the Diagnostics console, parsed from a Result-characteristic
/// notification shaped `diag:<kind>,<OK|ERR>,<text>` (see
/// ESP32_Firmware/diag_engine.ino). The `diag:` prefix keeps these out of
/// the Results list.
class DiagLine {
  final String kind;
  final bool ok;
  final String text;
  final DateTime time;

  /// True for lines the app itself generated (commands sent), so the
  /// console can show a `>` prefix for them.
  final bool sent;

  const DiagLine({
    required this.kind,
    required this.ok,
    required this.text,
    required this.time,
    this.sent = false,
  });

  /// Returns null if [raw] isn't a diagnostics line.
  static DiagLine? tryParse(String raw) {
    if (!raw.startsWith('diag:')) return null;
    final first = raw.indexOf(',');
    final second = first < 0 ? -1 : raw.indexOf(',', first + 1);
    if (second < 0) return null;
    return DiagLine(
      kind: raw.substring(5, first),
      ok: raw.substring(first + 1, second) == 'OK',
      text: raw.substring(second + 1),
      time: DateTime.now(),
    );
  }

  String get display => sent ? '> $text' : '[$kind] $text';
}

/// One reading of the live bench data, captured each time the firmware sends a
/// status update. Kept in memory (see `AppState.liveHistory`) for the live
/// graph, CSV copy and the PDF report summary.
class LiveSample {
  final DateTime time;
  final double supplyV;
  final double dutV;
  final double currentA;
  final double positionPct;

  /// NaN when the firmware doesn't report a real temperature (V2 reports a raw
  /// ADC voltage placeholder), so charts/CSV leave it out instead of lying.
  final double tempC;

  const LiveSample({
    required this.time,
    required this.supplyV,
    required this.dutV,
    required this.currentA,
    required this.positionPct,
    required this.tempC,
  });

  double get powerW => dutV * currentA;

  /// Same column set as the tester's own `/LOGS/TEST_*.CSV`.
  static const csvHeader =
      'TIME,SUPPLY_V,DUT_V,CURRENT_A,POWER_W,POSITION_PCT,TEMP_C';

  String toCsvRow() => [
        time.toIso8601String(),
        supplyV.toStringAsFixed(2),
        dutV.toStringAsFixed(2),
        currentA.toStringAsFixed(2),
        powerW.toStringAsFixed(2),
        positionPct.toStringAsFixed(0),
        tempC.isNaN ? '' : tempC.toStringAsFixed(1),
      ].join(',');

  static String toCsv(List<LiveSample> samples) =>
      [csvHeader, ...samples.map((s) => s.toCsvRow())].join('\n');
}

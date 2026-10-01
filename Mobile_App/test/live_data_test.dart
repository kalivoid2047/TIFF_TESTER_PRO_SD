import 'package:flutter_test/flutter_test.dart';
import 'package:tiff_tester_pro/models/live_sample.dart';
import 'package:tiff_tester_pro/models/module_profile.dart';
import 'package:tiff_tester_pro/models/nano_status.dart';

void main() {
  group('LiveSample', () {
    test('CSV has the same columns as the tester log and computes power', () {
      final s = LiveSample(
        time: DateTime.utc(2026, 1, 2, 3, 4, 5),
        supplyV: 12.04,
        dutV: 11.8,
        currentA: 2.0,
        positionPct: 42.4,
        tempC: 31.26,
      );
      expect(s.powerW, closeTo(23.6, 1e-9));
      final csv = LiveSample.toCsv([s]).split('\n');
      expect(csv.first, 'TIME,SUPPLY_V,DUT_V,CURRENT_A,POWER_W,POSITION_PCT,TEMP_C');
      expect(csv[1], '2026-01-02T03:04:05.000Z,12.04,11.80,2.00,23.60,42,31.3');
    });

    test('an unreported temperature (NaN) is left blank, not written as a number', () {
      final s = LiveSample(
        time: DateTime.utc(2026),
        supplyV: 12,
        dutV: 12,
        currentA: 1,
        positionPct: 0,
        tempC: double.nan,
      );
      expect(s.toCsvRow().endsWith(','), isTrue);
      expect(s.toCsvRow(), isNot(contains('NaN')));
    });
  });

  group('NanoStatus.fromJson', () {
    test('reads the protection, polarity and self-test fields', () {
      final s = NanoStatus.fromJson({
        'relays': [1, 0, 1, 0],
        'lim_a': 3.5,
        'lim_t': 85,
        'aux_to': 30,
        'can_st': 1,
        'kline_st': 0,
        'pol': 0,
      });
      expect(s.relays, [true, false, true, false]);
      expect(s.limMaxA, 3.5);
      expect(s.limTempC, 85);
      expect(s.auxTimeoutS, 30);
      expect(s.canSelfTest, 1);
      expect(s.klineSelfTest, 0);
      expect(s.relayActiveLow, isFalse);
    });

    test('older firmware without the new fields gets safe defaults', () {
      final s = NanoStatus.fromJson({'relay': 1});
      expect(s.relays, [true, false, false, false]);
      expect(s.limMaxA, 5.0);
      expect(s.limTempC, 0);
      expect(s.auxTimeoutS, 0);
      expect(s.canSelfTest, -1); // not run
      expect(s.klineSelfTest, -1);
      expect(s.relayActiveLow, isTrue);
    });
  });

  group('ModuleProfile relay names', () {
    test('survive a JSON round trip', () {
      const m = ModuleProfile(
        id: '1',
        name: 'Turbo',
        relayNames: ['Ignition', '', 'Fuel pump', ''],
      );
      final back = ModuleProfile.fromJson(m.toJson());
      expect(back.relayNames, ['Ignition', '', 'Fuel pump', '']);
    });

    test('records saved before relay names existed load with four blanks', () {
      final back = ModuleProfile.fromJson({'id': '1', 'name': 'Old'});
      expect(back.relayNames, ['', '', '', '']);
    });
  });
}

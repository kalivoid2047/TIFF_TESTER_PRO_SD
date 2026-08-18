// Basic smoke test: the app boots to the splash screen without throwing.
// BLE hardware interaction isn't exercised here — see
// Documentation/ROADMAP.md Phase 4 for hardware-in-the-loop testing plans.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:tiff_tester_pro/main.dart';

void main() {
  testWidgets('App boots to splash screen', (WidgetTester tester) async {
    await tester.pumpWidget(const TiffTesterApp());

    expect(find.text('Initializing...'), findsOneWidget);
    expect(find.byIcon(Icons.directions_car_filled), findsOneWidget);

    // The splash screen schedules a Future.delayed to navigate onward;
    // let it fire and settle so no Timer is left pending when the test
    // ends (flutter_test asserts against that).
    await tester.pump(const Duration(seconds: 2));
  });
}

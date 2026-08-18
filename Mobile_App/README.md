# TIFF TESTER PRO — Mobile App

Flutter/Android companion app for the TIFF_TESTER_PRO_SD bench tester.
Connects over BLE to the ESP32 controller and drives the bench workflow:
scan/pair, ECU power control, injector/coil/quick tests, results, and PDF
report export.

Implements the screens in [`../Documentation/UI_UX_SPEC.md`](../Documentation/UI_UX_SPEC.md)
against the BLE contract in [`../Documentation/API_PROTOCOL_SPEC.md`](../Documentation/API_PROTOCOL_SPEC.md).
See [`../Documentation/PROJECT_STATUS.md`](../Documentation/PROJECT_STATUS.md)
for what's real vs. still pending hardware.

## Structure

```
lib/
├── main.dart              App entry point, theme, Provider setup
├── ble/                    BLE GATT service wrapper (flutter_blue_plus)
├── models/                 NanoStatus, TestResult
├── state/                  AppState — app-wide connection/results state
├── theme/                  Dark theme matching the reference screenshots
├── screens/                One file per screen (see UI_UX_SPEC.md §4)
└── services/                PDF report generation
```

## Running

Requires the Flutter SDK (stable channel) and Android toolchain.

```bash
flutter pub get
flutter run
```

BLE scanning requires a physical Android device (BLE doesn't work in the
emulator) with location/Bluetooth permissions granted at runtime — the app
requests these before scanning (Android 12+ model, see
`android/app/src/main/AndroidManifest.xml`).

## Testing this against real firmware

The BLE GATT service this app talks to is implemented in
`../ESP32_Firmware/ble_service.ino`. Note that `RUN_INJECTOR_TEST`,
`RUN_COIL_TEST`, and `RUN_ALL_INJECTORS` currently always return
`NOT_IMPLEMENTED` — there is no injector/coil driver hardware built yet
(see `../Documentation/ROADMAP.md` Phase 1). The resistance/short-to-ground/
current-monitor quick tests (on the Tests tab) use the Nano's existing
sensors and return real results today.

## Verified

`flutter analyze` (0 issues), `flutter test`, and `flutter build apk --debug`
all pass as of this writing — see `.github/workflows/ci.yml` for the CI job
that checks this on every push.

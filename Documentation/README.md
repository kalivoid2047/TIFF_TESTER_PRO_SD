# TIFF_TESTER_PRO_SD

[![GitHub Repo](https://img.shields.io/badge/GitHub-kalivoid2047%2FTIFF__TESTER__PRO__SD-181717?logo=github)](https://github.com/kalivoid2047/TIFF_TESTER_PRO_SD)

Bench-first automotive module tester/diagnostic/service platform.

## Controller roles

### ESP32
- Main application controller
- Wi-Fi AP / web interface
- SD module database
- Module upload/validation
- Future Android/Bluetooth interface
- CAN/K-Line diagnostic engines

### Arduino Nano
- Independent safety controller
- DUT relay control
- Voltage/current supervision
- Emergency-stop supervision
- ESP32 heartbeat supervision
- Hardware watchdog

## Safety behavior

DUT power is OFF by default. The Nano removes DUT power when:
- emergency stop is pressed
- supply voltage is outside configured limits
- DUT voltage is too high
- current exceeds configured limit
- ESP32 heartbeat is lost
- Nano hardware watchdog resets the controller

## Bench workflow

1. Select module profile.
2. Verify bench wiring.
3. Run pre-power safety checks.
4. Request DUT power.
5. Nano performs independent safety checks.
6. Run module tests.
7. Diagnose communication faults if supported.
8. Run validated service/calibration procedures where supported.
9. Save test report.

## Web interface

ESP32 creates:
SSID: TIFF_TESTER
Default AP address: 192.168.4.1

The web page includes module-profile paste/upload and basic safety status.

## Important

The supplied firmware is a foundation for the project, not a finished OEM diagnostic implementation. Exact connector pinouts, voltage-divider ratios, current-sensor calibration, CAN transceiver wiring, K-Line wiring, and OEM diagnostic/service procedures must be validated on the actual hardware/module before connecting expensive automotive ECUs or actuators.

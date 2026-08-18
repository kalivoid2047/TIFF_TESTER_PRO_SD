# TIFF_TESTER_PRO_SD

[![GitHub Repo](https://img.shields.io/badge/GitHub-kalivoid2047%2FTIFF__TESTER__PRO__SD-181717?logo=github)](https://github.com/kalivoid2047/TIFF_TESTER_PRO_SD)

Bench-first automotive module tester/diagnostic/service platform, built around
an ESP32 main controller and an independent Arduino Nano safety controller.

## Repository layout

```
TIFF_TESTER_PRO_SD/
├── Arduino_Nano_Safety/    Nano safety controller firmware
├── ESP32_Firmware/         ESP32 main application firmware
├── SD_MODULES/             Module profiles stored on the SD card
└── Documentation/          All project documentation (start here)
```

## Documentation

Full project documentation lives in [`Documentation/`](Documentation/):

- [README.md](Documentation/README.md) — architecture overview, safety
  behavior, bench workflow, web interface
- [PROJECT_STATUS.md](Documentation/PROJECT_STATUS.md) — as-built status of
  the current firmware
- [PRD.md](Documentation/PRD.md) — product requirements
- [SRS.md](Documentation/SRS.md) — software requirements specification
- [ARCHITECTURE.md](Documentation/ARCHITECTURE.md) — system architecture
- [UI_UX_SPEC.md](Documentation/UI_UX_SPEC.md) — mobile app UI/UX spec
- [API_PROTOCOL_SPEC.md](Documentation/API_PROTOCOL_SPEC.md) — BLE/Wi-Fi/UART
  protocol contracts
- [ROADMAP.md](Documentation/ROADMAP.md) — phased development roadmap
- [PINOUT_AND_WIRING.txt](Documentation/PINOUT_AND_WIRING.txt) — inter-
  controller and sensor wiring

## Important

The supplied firmware is a foundation for the project, not a finished OEM
diagnostic implementation. Exact connector pinouts, voltage-divider ratios,
current-sensor calibration, CAN transceiver wiring, K-Line wiring, and OEM
diagnostic/service procedures must be validated on the actual
hardware/module before connecting expensive automotive ECUs or actuators.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) — please read the safety-critical
code section before touching `Arduino_Nano_Safety/`. Participation in this
project is governed by the [Code of Conduct](CODE_OF_CONDUCT.md).

## License

MIT — see [LICENSE](LICENSE).

# Contributing to TIFF_TESTER_PRO_SD

Thanks for your interest in this project. This is a bench-first automotive
diagnostic tester with a hardware safety layer — please read the safety note
below before submitting changes, especially anything touching the Nano
firmware.

## Before you start

- Read [Documentation/README.md](Documentation/README.md) for the project
  overview and [Documentation/PROJECT_STATUS.md](Documentation/PROJECT_STATUS.md)
  for the current as-built state.
- If you're proposing new functionality, check
  [Documentation/ROADMAP.md](Documentation/ROADMAP.md) first — it may already
  be planned with dependencies/open decisions noted, and
  [Documentation/PRD.md](Documentation/PRD.md) / [Documentation/SRS.md](Documentation/SRS.md)
  describe the intended scope.
- For anything touching the ESP32↔Nano protocol or the mobile app, check
  [Documentation/API_PROTOCOL_SPEC.md](Documentation/API_PROTOCOL_SPEC.md)
  and [Documentation/UI_UX_SPEC.md](Documentation/UI_UX_SPEC.md) so changes
  stay consistent with the documented contracts.

## Safety-critical code — extra care required

The Arduino Nano firmware (`Arduino_Nano_Safety/`) is the independent safety
supervisor for this system. It is the **only** component that may energize
any DUT-facing output, and it must remain trustworthy on its own — never
dependent on the ESP32, Wi-Fi, BLE, or the app being correct or even present.

If your change touches `Arduino_Nano_Safety/`:
- Never remove or weaken a safety check (e-stop, voltage/current limits,
  heartbeat timeout, watchdog) without an explicit discussion in your PR
  description of why it's safe to do so.
- New DUT-facing outputs (e.g. additional driver channels) must go through
  the same independent validation as the existing relay — see
  [Documentation/ARCHITECTURE.md](Documentation/ARCHITECTURE.md) §3 for the
  non-negotiable safety boundary.
- Bench-test any change against real hardware (or at minimum a simulated
  fault: pull e-stop, disconnect the UART link, force an out-of-range ADC
  reading) before submitting — don't rely on "it compiles."
- Calibration constants (voltage dividers, current-sensor scaling) are
  placeholders in the current firmware and are explicitly marked as such —
  don't silently change them without noting it's a calibration update, not a
  logic change.

## How to contribute

1. Fork the repo and create a branch off `master` (`git checkout -b
   my-change`).
2. Make your change. Keep commits scoped and describe *why*, not just *what*,
   in the commit message.
3. If your change affects behavior described in any `Documentation/*.md`
   file, update that document in the same PR — the docs are meant to stay
   accurate, not aspirational.
4. Open a pull request describing:
   - what changed and why
   - what you tested it against (bench hardware, simulation, etc.)
   - any safety implications, if touching `Arduino_Nano_Safety/`
5. Be responsive to review feedback — this is a small project, review may
   take a little while.

## Code style

- Match the existing style in the file you're editing (naming, comment
  density, structure) rather than introducing a new convention.
- Arduino/C++ (`.ino` files): keep functions small and single-purpose,
  consistent with the current firmware's style; prefer explicit constants
  over magic numbers (see `Arduino_Nano_Safety/TIFF_TESTER_PRO_SD_NANO.ino`
  for the existing pattern).
- Module profiles (`.INI` under `SD_MODULES/`): follow the schema
  demonstrated in `SD_MODULES/MODULES/TOYOTA/HILUX_1KD_TURBO.INI` and
  documented in [Documentation/SRS.md](Documentation/SRS.md) §6.1.
- Documentation: Markdown, matching the tone/structure of existing files
  under `Documentation/`.

## Reporting issues

Open a GitHub issue with:
- What you expected vs. what happened
- Hardware/firmware version if applicable
- Steps to reproduce

For anything that looks like a safety defect (a fault condition that fails
to cut DUT power, for example), please flag it clearly as a safety issue in
the title.

## License

By contributing, you agree that your contributions will be licensed under
the project's [MIT License](LICENSE).

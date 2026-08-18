## What changed

<!-- Describe what this PR changes and why. -->

## Related docs / issues

<!-- Link any relevant issue, or the Documentation/*.md section this
     implements (e.g. Documentation/ROADMAP.md Phase 2). -->

## Testing

<!-- What did you test this against, and how? Bench hardware, a
     simulated fault condition, CI compile only, etc. Be specific —
     "it compiles" is not sufficient for firmware changes. -->

-

## Safety-critical checklist

**Required if this PR touches `Arduino_Nano_Safety/`** (delete this
section otherwise):

- [ ] No existing safety check (e-stop, voltage/current limits, heartbeat
      timeout, watchdog) was removed or weakened without explanation below
- [ ] Any new DUT-facing output goes through the same independent
      validation as the existing relay (see
      [Documentation/ARCHITECTURE.md](../Documentation/ARCHITECTURE.md) §3)
- [ ] Tested against a real or simulated fault condition (e-stop pulled,
      UART link dropped, out-of-range ADC reading), not just a compile
- [ ] Calibration-only changes are clearly labeled as such, separate from
      logic changes

If any box is unchecked, explain why below:

## Documentation updated

- [ ] I updated the relevant `Documentation/*.md` file(s) if this PR
      changes documented behavior (see
      [CONTRIBUTING.md](../CONTRIBUTING.md))
- [ ] N/A — no documented behavior changed

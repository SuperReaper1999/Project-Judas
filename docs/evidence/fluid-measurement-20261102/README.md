# Fluid measurement evidence — 2 November 2026

Read [the measurement audit](../../FLUID_MEASUREMENT_AUDIT.md). Base is
`42302816aaf5d6fbeb577f6daf9a3112ff7ceaf4`; this containing checkpoint records
test-only instrumentation, geometric diagnostics and current documentation.
No production fluid implementation, independent research or Alpha policy changed.

`evidence.tar.gz` contains complete baseline/captured inputs and raw output,
including all before/endpoint particle/body/ownership snapshots for the original
and late windows. It includes source copies, native/Python build and test logs,
force-budget and geometric reports, forged-owner/input controls and original
failed production output. Each extracted member was read back and checked against
`ARCHIVE_MANIFEST.json`. No executable, static library or large build tree is
archived. `FINGERPRINTS.json` records actual binary and source hashes separately.

The direct production/replay drivers do not expose `--build-info-json` in this
configuration. The configured build identity is labelled as configuration, not
substituted for binary identity. The original retained baseline executable did
expose it; its historical response is preserved. These were working-source
diagnostic builds, not a claim of final tagged-release provenance. Build and link
inputs can be regenerated from the containing source and the documented commands.

Key records outside the archive:

- `PHYSICAL_PARITY.json`: 720 identical non-timing physical/diagnostic rows.
- `PARTICLE_POSE_PARITY.json`: eight byte-identical complete original snapshots.
- `FORCE_BUDGET.json`: both full windows and actual accounting residuals.
- `REGRESSIONS.json`: eight focused native invocations, all exit zero.
- `NEGATIVE_CONTROLS.json`: four replay ownership/input controls.

The native geometric kernel passes 84 controls and the Python budget tool five.
Registration checks pass seven with two existing environment skips. The original
production run exits **1**, retaining its one acceptance failure and failed
historical four-second speed comparison; it is not included among passing checks.
The geometric cell proxy is explicitly rejected as a continuum-immersion oracle.

## Reproduce

Extract into a fresh disposable directory. `captured/` can be replayed directly
with `tools/validation/audit_fluid_force_budget.py`. The coverage reports retain
the original exact commands; replace their old checkout prefix with the current
root and use the archived matching snapshot paths. For a fresh native run, build
the targets and use commands in the main audit. The production fixture's expected
exit remains 1. Do not change the Alpha lock to execute it: use Experimental.

Archive sources/commands retain chronology, including the initial new-target
build request before CMake was reconfigured. That setup error was resolved by
configuration, without modifying an assertion. No Windows/Android or human
validation is claimed for this diagnostic continuation.

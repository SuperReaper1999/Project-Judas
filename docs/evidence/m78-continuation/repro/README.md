# Exact M77 in-progress-step continuation failure

`m77-native-in-progress-step-failed.save` is the original native Windows saved
numerical state, preserved byte-for-byte (52,557 bytes; SHA-256
`a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b`).
It is failure evidence, not an executable or temporary package. The adjacent
identity JSON records its original executable/archive/fixture provenance.

The unchanged application fixture is `tests/BalanceLabTests.cpp`, SHA-256
`5ba5cb47fe7ce084097391fba84a9086e84dc9dd284efce83880021b3fc40a6f`.
The ordinary input project is `projects/balance_lab`; its original script,
scene, rig and controls must remain unchanged. The test uses its authored J
impulse, an in-progress step snapshot, and the original 60 Hz/720-step resumed
standing check. No thresholds, gains or simulation cadence are changed.

## Linux reproduction

From the repository root, use a fresh owned output directory:

```sh
mkdir -p .cache/m78-continuation/repro-native-step/slots
cp -a projects/balance_lab .cache/m78-continuation/repro-native-step/project
cp docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save \
  .cache/m78-continuation/repro-native-step/slots/m77.save
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  .cache/m78-followup/release/judas_balance_lab_tests \
  .cache/m78-continuation/repro-native-step cold-read
```

Use the matching candidate build/archive; the original Linux receipt is
`../linux-read-native-step.json` (13 checks / 1 failure). An existing populated
output directory must not be reused for a new experiment. Verify the save hash
before and after the run.

## Native Windows reproduction

From the verified candidate source root, with its compiled native test and
runtime DLLs available:

```powershell
$owned = 'C:\JudasValidation\M78ContinuationResults\repro-native-step'
New-Item -ItemType Directory -Path "$owned\slots" | Out-Null
Copy-Item projects\balance_lab "$owned\project" -Recurse
Copy-Item docs\evidence\m78-continuation\repro\m77-native-in-progress-step-failed.save "$owned\slots\m77.save"
$env:SDL_AUDIODRIVER = 'dummy'
.\BuildRelease\judas_balance_lab_tests.exe $owned cold-read
```

The original native receipts are
`../m78-combined-v2-run-balance_lab-cold-write.json` and
`../m78-combined-v2-run-balance_lab-cold-read.json`.
The save's recorded metadata/content identity is intentionally unchanged.

## Discriminating observations

- Fresh capture reproduced the identical save SHA. Continuing that exact live
  captured state uninterrupted reaches standing; a fresh-process restore fails.
- All seven dynamic mapped bodies' physical state, shapes, mass/inertia,
  filters/materials/force accumulators; all mapped joint settings; and the
  controller's semantic history are exactly preserved. Reallocated native
  handles are compared through durable entity identities.
- Angular-drive commands are genuinely omitted on restore. A separate ordinary
  Ball-joint reproduction proves the missing first-step actuator response, but
  replaying only the actual captured commands does **not** repair this fixture.
- Removing the first restored `Prepare` return aligns the clocks/evaluates normal
  current-state commands, but its isolated Linux run also fails. It is not a
  proposed production fix.

See `../native-cold-state-roundtrip-differences.json`,
`../m78-combined-v2-state-isolation.json`, `../cold-wait-isolation.json`, and
`../m78-combined-v2-drive-isolation.json` for exact source identities, commands,
first-step traces and retained outcomes. Solver warm histories and completed
contact observations are distinct; no missing contact load is invented or
serialized as part of these observations.

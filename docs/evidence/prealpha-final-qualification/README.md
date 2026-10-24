# Final Pre-Alpha qualification evidence

See [report](../../PREALPHA_FINAL_QUALIFICATION.md) and
[support matrix](../../ALPHA_SUPPORT_MATRIX.md). Initial exact accepted source:
`c76e92dc20b5765f61d6077bf618a8b94ac22462`. No main promotion or Alpha publication.

- `accepted-sdk/`: exact clean c76 tool inventory/hashes, older-loader receipt,
  editor/Play-Stop/export logs and rendered frames, 19 original CLI operations,
  extra real tool operations and relocation commands. All 17 executables have
  actual workflows. Its physics qualification still has the failures below.
- `builds/`: full root package versions, clean initial configure/build/qualification,
  and the later dirty-candidate incremental build/ABI receipts. Initial build
  engine/tools/tests use only tracked source; prepared Ubuntu tooling is reused.
- `qualification/RESULTS.json`: 41 original invocations; **not all pass**. Wheel has
  two physical failures, legacy spacecraft has three incorrect fixed-omega oracle
  failures, and CharacterMotor hits a read-only fixture output path.
- `qualification-followup/RESULTS.json`: all 29 affected invocations pass after the
  generic external-kick correction, independent angular-momentum oracle and owned
  test-output redirection. No thresholds/controller/cadence/budgets are weakened.
- `*-raw.tar.gz`: original test output, JSON, CSV and actual generated saves,
  without executables/objects/libraries or unrelated source assets.
- `original-source/`: byte-preserved three input files, including original FTFT7
  session test and its failing oracle. Original protected evidence/research is
  not edited. The active oracle now checks physical torque/angular momentum.
- `WHEEL_COMPARISON.json`, `wheel-before.log`, `wheel-current.log`: same GCC15
  diagnostic driver against frozen before/candidate engine archives. This separates
  a runtime regression from compiler differences; not a timing benchmark.
- `prototype/`: the small acceleration-timing prototype passes original wheel
  checks. `kick-negative-control.log` proves the new generic trajectory controls
  fail against the accepted engine before the correction.
- `TESTED_SOURCE.json`: actual dirty test-build identity, three changed compiled
  inputs, and reverified final follow-up binary receipts. It is not relabelled as
  a later clean commit binary. `runtime-test-correction.patch` reproduces the diff.
- `reproduction/`: actual build, serial run, SDK/tool and package-only launch
  drivers. Scratch host paths need replacing on another machine. The old rootfs
  prerequisite is documented in `docs/LINUX_BASELINE_BUILD.md`.
- Packaging/distribution/API/link check logs retain their precise coverage scope.

## Genuine setup mistakes retained

The SDK test helper rejected overlapping source/output mounts before running.
Its corrected sibling layout is recorded in `SDK_SETUP_FOLLOWUP.json`. A minimum
8/8 environment-bake probe was rejected; the documented 16/32 parameters work.
CharacterMotor requires its owned relative `build/m49-focused` output to be writable.
None of these is silently counted as an engine pass or a numerical defect.

## Boundaries

Ubuntu24 rootless userland, Mesa llvmpipe, Xvfb and dummy audio do not certify a
physical GPU/audio device. Native networking is real local multi-process TCP, not
Windows/Android/phone/public-Internet acceptance. The corrected runtime remains
Experimental for human review. FLU-001, half-turn liquid sampling, old incomplete
save histories and unsupported system combinations remain explicit limits.

A clean committed SDK receipt follows the validated implementation checkpoint;
it will identify its exact source and binaries independently of these earlier
records. The exact-c76 SDK remains preserved rather than overwritten.

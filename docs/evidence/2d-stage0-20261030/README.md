# Native 2D Stage 0 evidence

Read `docs/2D_STAGE0.md` for the decision, mathematical contracts and boundaries.
This is a private prototype checkpoint. Production source and public APIs are
unchanged. Reproduction commands in that report generate isolated MK2 source
copies; they never patch live `src/` or an independent research directory.

- `algebra/`: final 74-check algebra/camera probe and missing-row control.
- `native/`: final combined qualification, raw logs and small native save/trace
  fixtures. `RESULTS.json` classifies 28 runs; not all are passing assertions.
- `BASE_SOURCES.json`: unchanged native source/dependency hashes, including
  original regression test source. GLM and toolchain are external prerequisites.
- `GENERATED_SOURCES.json`: complete generated src hashes for each build.
- `ROWS_GENERATED.patch`, `REDUCED_GENERATED.patch`: exact generated source
  changes against the baseline; generator sources live in `tools/prototypes/`.
- `PROVENANCE.json`: production baseline, native Linux compiler/hardware/scope.
- `FINAL_FINGERPRINTS.json`: final repository-owned inputs and evidence hashes.
- `preserved/`: original setup failures, first native sleep-integration omission,
  first near-aligned 3D hinge failure and the initial algebra results/source.
  The original algebra source is restored and hash-verified against its original
  receipt; it is historical, not the final probe to run.

## Expected failures — never count these as passes

`native/rejected-native-rows.log` fails the original off-centre floor tolerance.
The constrained competitor also allows forbidden pre-step impulse velocity.
`native/legacy-hinge-counterexample.log` fails the spin-retention contract in a
dedicated build with only the new unsupported-joint guard removed.
`*-strict.log` records failed byte-identical cold continuation for both planar
approaches and ordinary 3D. Physical endpoint checks pass separately. The initial
strict failure is retained, not relabeled. `*-ablation-*` proves the contact warm
cache mechanism; clearing warm history is diagnostic only, never the candidate.

The qualifier requires expected failure diagnostics, not just nonzero exit codes.
Missing-row algebra control likewise must fail at its named physical contract.
No test was deleted, no original threshold weakened, and no solver budget changed.

## Interpretation

The 3D trajectory digest compares real pose/velocity samples at every step, not
controller status. The cold fixture restores real native participants in fresh
processes; it is not a production RuntimeWorld scene-save/export acceptance.
Performance covers moving independent 3D proxy bodies, not dense 2D collisions.
Sphere mass properties remain 3D in native proxy cases; analytic 2D lamina/disk
contracts are a separate concern. No playable sprite project, rendering capture,
Windows/Android, SDK, editor Play/Stop or full-suite qualification is claimed.

# Bounded connected-ownership correction

Read [the outcome and remaining mechanism](../../FLUID_CONNECTED_MATERIAL_OWNERS.md).
This is a representation/projection checkpoint, not a new usable fluid backend.
The original fixtures still fail admission; no native body advances here.

## Reproduce

From this source checkpoint, with C++17, Bash and the existing GLM dependency:

```bash
bash docs/evidence/fluid-connected-owners-20261105/reproduce.sh /tmp/judas-connected-owners
```

Expected current outcome: **33 controls pass, both authored proposals reject,
exit 1**. The driver does not convert these rejections into success.

The retained controls can be rerun separately:

```bash
bash tools/prototypes/fluid_volume_grid/qualify_moving_grid.sh /tmp/judas-retained-owners
python3 docs/evidence/fluid-connected-owners-20261105/phase_window_oracle.py
sha256sum -c docs/evidence/fluid-connected-owners-20261105/SOURCE_SHA256SUMS.txt
(cd docs/evidence/fluid-connected-owners-20261105 && sha256sum -c SHA256SUMS.txt)
```

The old evidence at `docs/evidence/advanced-fluids-v2-20261105/` is unmodified.
Its source manifest describes its original commit `61c741a7`, not this changed
source tree. Use the appropriate source checkpoint when verifying old source
hashes; do not rewrite historical hashes to match newer code.

## Raw records

- `qualification.json`: exact parent, compiler, machine, flags and disposition.
- `controls.log`: 33 new compiled controls, all passing.
- `controls-sanitized.log`: the same 33 controls under ASan/UBSan, compiled with
  `-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined`, run with
  `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
  Leak detection was disabled; no leak-detector qualification is claimed.
- `retained-admission.log`: the preceding 149 admission/negative controls, still
  passing with the modified scalar graph entry point.
- `retained.log.gz`: all 1,030 existing checks across 26 suites, unchanged and
  passing. Exact raw stdout, including trailing spaces, is retained in gzip.
- `scalar-parent.log` / `scalar-new.log`: byte-identical pressure values,
  residuals and iteration counts for 48 deterministic graph cases. Build
  `ScalarGraphDigest.cpp` against this graph and the graph source from parent
  `61c741a7` using the recorded normal flags. This checks the scalar API; it is
  not a native rigid-body trajectory comparison.
- `authored.log` / `.exit`: final original-fixture proposals with unchanged
  imported float geometry, water, gravity, timestep and top vent. Both fail
  transactionally. Initial body pose is frozen. Pressure closure passes; phase
  transport rejects before publication. Recorded negative gas is an unsupported
  **trial**, not a published state.
- `resource.log`: diagnostic elapsed time and process RSS for that two-case
  probe. This is not an optimized-step benchmark, and no speedup is claimed.
- `phase_window_oracle.py` / `phase-window.json`: independent exact-arithmetic
  straight-tube displacement oracle. It demonstrates why a frozen outlet phase
  can close the EOS while producing negative gas. This is not engine acceptance
  or a replacement solver, and is not counted among the 33 passing controls.

## Retained failed variants

The early `basin*.log.gz` records preserve the successive attempted gates:
initial global minimum tolerance, per-owner tolerances, seam aggregation, and
matching the inner linear budget to the independently audited final budget.
They are diagnostic observations, not matched performance comparisons.

`rejected-variant-sources.tar.gz` preserves the original per-patch seam classifier
and the projection with the unnecessarily stricter inner budget. The earlier
exploratory logs do not all have complete byte-identical build snapshots;
no such provenance or performance equivalence is claimed. The final source is
fully hashed and reproducible with the command above.

The unchanged physical acceptance checks still reject both original fixtures.
No geometry was rounded away, no water or gas quantity clamped into validity,
and no main/release policy file was modified.

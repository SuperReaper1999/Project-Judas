# Repeated impact response: evidence

Read [the investigation report](../../PREALPHA_IMPACT_RESPONSE_INVESTIGATION.md)
first. **No new production repair or performance pass is claimed.** Runtime
sources/binaries remain at Experimental `6059bafb`.

## Contents

- `cases/`: ten exact pre/post-event body/contact states from original step 347,
  losslessly compressed. No replacement fixtures or tuned thresholds.
- `observer/`: private read-only capture overlay, actual build recipe/receipt,
  build output and all-347-step parity result. Capture timing includes JSON work
  and is not a performance measurement.
- `replay/`: offline native response replay, body/contact serialization,
  independent three-body controls, results and build commands/hashes. Old private
  revisions and original compiler-warning log remain alongside the final clean
  build log. These diagnostics are not active release tests requiring future
  solvers to preserve the known defect.
- `reproduced/`: independently rerun results using the archived evidence source
  and the exact unmodified engine archive; `reproduce-evidence.log` records it.
- `rejected-dispatch/`: actual tested dispatch-certificate prototype, recipe,
  build receipt and retained earlier-run comparison. The authoritative fresh
  serial comparison is `COMPARISON.json`; do not substitute the older retained
  baseline as a new run.
- `runs/`: full CSVs (gzip), runtime logs, resource usage, context, commands and
  summaries for capture347, fresh current400, prototype400 and both visible
  35-second runs. Prototype diagnostics print one bounded acceptance count per
  applicable step; that logging is included in prototype timing.
- `FINAL_VERIFICATION.json`: production source/binary identity and branch status.
- `PARENT_FROZEN.json`: as-recorded source, compiler, hardware and binary receipt
  for the current frozen executable. Its embedded Git parent is `b7db703c`:
  candidate executable sources were subsequently committed unchanged as
  `6059bafb`. The earlier documentation-only `WorldModules.md` delta is explicit.
- `MANIFEST.json`: hashes of every evidence file, including uncompressed case
  payload hashes. No engine binaries or unrelated assets are copied here.

## Mechanism controls

1. Capture reproduces every physical/work field for all 347 steps.
2. Full offline replay matches actual native velocities bit-for-bit in ten cases.
3. Removing only selected rows leaves all 751 changed outputs identical in event
   28; the seed alone produces zero response. Event 18 preserves 807 of 810
   changes exactly without the seed.
4. Setting incoming velocities to zero changes zero bodies in all ten cases.
5. A genuine equal-mass three-body impact must propagate to the third body.
6. Subtracting the old-contact projection from a full projection produces a
   1 m/s contact violation in an independent frictionless chain. This rejects
   the proposed shortcut; it is not a claimed defect in the unmodified solver.
7. The prototype's full 400-step and live runs complete, but do not resolve the
   scaling failure or establish equivalent physical quality. It is not adopted.

The table covers a deliberately selected ten-event sample, not every impact.
Mixed linear/angular velocity norms in raw JSON have mixed units; no physical
energy or causal percentage is inferred from those norms. Energy guards passing
alone are insufficient collision correctness evidence.

## Reproduce offline native cases

From the repository root, with the existing GNU Makefiles Release build:

```sh
python3 docs/evidence/prealpha-impact-response-20261021/reproduce.py \
  --build build \
  --core .cache/event-architecture-20261021/final-snapshot/libjudas_engine.a \
  --output /tmp/judas-impact-response-replay
```

Choose a fresh output directory. The helper compiles only the two diagnostic
translation units, explicitly links the chosen engine archive and records its
hash. It does not modify the engine/build products or overwrite evidence. The
original archive hash is
`c724dbbba56538f0fe3b5017dab12f3bb928facdb9b0add21d77297964698eae`.
If that local archive is unavailable, an explicitly selected rebuilt archive can
produce a new observation; the helper reports that it is a different binary.
Do not call it verification of the originally measured executable. It requires
an already configured/built `judas_impact_solver_tests` link recipe and its
normal dependencies, not a new permanent dependency.

## Reproduce the actual workload

The original unmodified frozen measurement inputs remain at
`.cache/event-architecture-20261021/final-snapshot/`; the private observer and
prototype builds remain under `.cache/impact-response-20261021/`. They are
ignored local artifacts. Their original build recipes preserve absolute local
paths and are receipts, not claims of a portable prebuilt SDK.

```sh
python3 scripts/physics_dense_measure.py run \
  --snapshot .cache/event-architecture-20261021/final-snapshot \
  --output /tmp/judas-dense-current400 \
  --mode exact --steps 400 --seconds 180 --timeout 240 --profile 0

python3 scripts/physics_dense_measure.py run \
  --snapshot .cache/event-architecture-20261021/final-snapshot \
  --output /tmp/judas-dense-current-live35 \
  --mode realtime --seconds 35 --timeout 90 --profile 0 --visible
```

Native desktop access is required. Run serially. Exact mode does not measure
output FPS. Live frame comparisons must disclose the chosen wall interval and
simulation progress, not just count fast startup frames. The 400-step prototype
changes trajectories from step 153 and is not an exact physics comparison.
There is no 600-step/full-regression qualification of this rejected prototype.

An initial private compile used C++20 `bit_cast` in this C++17 build and failed;
`memcmp` corrected the diagnostic comparison. The initial tool error remains in
the chat record rather than a reconstructed compiler log. An intermediate
three-body control had unused-result warnings; its actual log is preserved,
and the final build is warning-free. Neither setup issue is an engine failure.

No full production suite, Windows VM, Android build, export or human acceptance
is claimed for this investigation. No current engine source changed, so existing
qualified production behavior is retained. Protected research/history is untouched.

# Combined final-source result — 2026-11-01

Uncommitted Experimental candidate based on 90a97547. Main remains 49c4f3d9.
Alpha Advanced Fluids remain locked. No acceptance settings were changed.

## Correctness checks

| Check | Result |
|---|---|
| Vertical geometry, remote-donor continuity, equivalent solid frames | 317 checks, zero failures |
| Hydrostatics, including recorded rotated force covariance | 124 checks, zero failures |
| Lateral wet support | 109 checks, zero failures |
| Window/decay validator | 46 checks, zero failures |
| Independent calm model/native physical controls | 81 checks, zero failures |
| Swimming | 89 checks, zero failures, 2,475 steps |
| Project fluid policy | 26 checks, zero failures |
| Fluid lifecycle | 21 checks, zero failures |
| Full production qualification | 13,801 checks, three mandatory failures, one scoped diagnostic failure |
| Original four-second mode | 8,021 checks, two mandatory failures, one scoped diagnostic failure |

All 2,640 non-timing physical prefix rows and 66 body/particle snapshot pairs
match between the final candidate's default and legacy modes. Corrections change
trajectories relative to the pre-fix implementation; those older traces remain
preserved and are not claimed identical to the corrected physics.

## Before/after window observations

| Case | Late peak before / after (m/s) | Late RMS before / after (m/s) | Late range before / after (m) | Late sampled / plane immersion after |
|---|---|---|---|---|
| half_020 | .376893 / .375252 | .129339 / .150201 | .098923 / .115878 | .504679 / .593405 |
| half_025 | .630705 / .530423 | .321804 / .187565 | .244748 / .132042 | .512439 / .595371 |
| rotated_half | .478254 / .441299 | .179188 / .182092 | .132367 / .143646 | .499848 / .621468 |
| light_80kg | .443885 / .382859 | .213154 / .139215 | .142805 / .112682 | .505664 / .576520 |

Do not describe every measurement as improved: half_020 late RMS/range increase,
and rotated late RMS/range slightly increase. All RMS/range decay gates pass.

The remaining mandatory production failures are:

1. half_025 late peak **0.5304233432 m/s**, against strict **0.5 m/s**.
2. half_025 original 3–4 s plane-estimated immersion **0.6019317627**, against
   **0.5 +/-0.10**. This particular observation passed on the pre-fix trajectory;
   it now fails. The observation method is independently demonstrated unreliable
   for sloped/nonplanar surfaces, but this assertion remains active.
3. rotated_half late plane-estimated immersion **0.6214682261**, same tolerance.

The diagnostic sub-particle-size legacy settling comparison still fails; supported
scope was not expanded to disguise it. The legacy mode still reports the coarse
original immersion failure and the light_80kg four-second speed failure. Original
failed artifacts remain separately preserved.

## Tests that detect absent/broken mechanisms

Against the final 317-check fixture, the pre-task reconstruction produces **34
failures**. The field-frame-only intermediate produces **one** failure in the
existing 124-check hydrostatic suite (recorded force covariance). Testing actual
rather than rounded motion-donor positions corrects that failure. The rejected
all-query-slicing experiment still produces **three** established column-mean
failures. Fresh raw negative-control runs and exact source hashes are included.

Manufactured observer controls independently prove that a plane through the
center of a box has half its volume submerged while the old remote flat-plane
estimate may report 0.828125, 1 or 0. The calm flat control remains correct.
This identifies an oracle limitation, not permission to remove failed checks.

## Reproduction

Use `run/final/RESULTS.json` for exact commands and binary fingerprints. Source
snapshots include the complete pending fluid candidate and test-registration
files, not just this turn's changes. The combined test driver is unchanged from
the window-contract candidate. `TASK_PATCH.diff.gz` isolates this task's main
implementation/test changes. Build with the existing Linux Release configuration:

    cmake --build .cache/2d-stage1-build --target judas_production_fluid_tests judas_fluid_vertical_support_tests judas_fluid_hydrostatics_tests judas_fluid_wet_support_tests -j2
    .cache/2d-stage1-build/judas_production_fluid_tests --output /tmp/fluid-final
    .cache/2d-stage1-build/judas_production_fluid_tests --legacy-four-second --output /tmp/fluid-legacy-final

The production commands are expected to exit nonzero. The reproduction scripts
in run/ retain intermediate compilation/link recipes and snapshot replay inputs.

## Remaining work and review boundary

- Establish a qualified local wet-volume oracle for nonplanar surfaces and rotated
  displaced solids; preserve the old measurements alongside it.
- Trace the remaining coarse 0.5304 m/s peak without widening the threshold or
  adding arbitrary drag. Improved motion is not a completed settling correction.
- Reduce the cost of per-height geometric ownership while retaining its continuity
  and frame-equivalence contracts. Coarse executed coupling median rose from
  10.88 to 18.23 ms in these diagnostic same-authored-workload runs; trajectories
  differ, so this is not a matched-trajectory performance benchmark.
- Further physical qualification, including flat/radial scenes and cold-process
  continuation, remains necessary before unlocking Advanced Fluids.
- Windows/Android/hardware and visual water appearance were not qualified here.

Human review checklist: compare flat/radial buoyancy without launch; check coarse
late bobbing rather than only a four-second endpoint; confirm Basic Water remains
independent; do not unlock or promote this candidate based on these results.

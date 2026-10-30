# Native 2D Stage 2 qualification

Native Linux Release work from Experimental
`3916ff5b57b75503f8f2d770962407c39b9c0290`, 2026-10-30.
This is automated evidence, not human acceptance or a v1.1.0 release.
Main remains `e2008d1e6f4ac3229b292724e5718f37e8d2e1ed`.

`RESULTS.json` is the authoritative focused gate run with exact binary identities,
commands, source fingerprints, timings and outcomes. `SOURCE_SHA256SUMS` records
the intended implementation, tests, documentation and lab source. It deliberately
excludes itself/evidence outputs. `BINARY_SHA256SUMS` identifies local validated
executables; binaries/build products are not committed. Builds made before the
checkpoint honestly identify the parent commit plus dirty/source fingerprints.
They are not falsely labelled as a binary built from a later commit.

Selected gates: planar physics/integration; Stage 1 sprite math/integration/render;
collision geometry; CharacterMotor; physics continuation; MK2 physics/rotation/
cold rotation. Supplementary logs cover native joints/kinematics, wheels,
powertrains, hierarchy creation and isolated contact predicates. The fresh-process
planar restore is a separate invocation. `native-repeat.log` is byte-identical to
the native gate output on this machine/binary. This is not cross-platform bitwise
certification or a complete engine-suite rerun.

## Preserved unsuccessful states

- `resting-before.log`: a tilted box retained only the deepest corner instead of
  clipping the incident edge. Independent transformed vertices exposed roughly
  29 mm penetration. The final manifold keeps each penetrating corner's own gap;
  the unchanged 2 mm single-body gate passes.
- `rolling-before.log`: a rotating circle's stale material anchor was treated as
  its continuing support feature, producing about 19 mm sinking. A diagnostic
  also failed on one unsplit straight segment, ruling out seam adjacency as the
  cause. Planar finite/rounded manifolds now refresh during substeps, preserving
  existing solver budgets/damping; the chain-seam fixture stays at radius height.
- `friction-before.log`: arbitrary spatial tangent vectors become redundant after
  planar reduction. The spatial two-axis Coulomb limit therefore depended on the
  motion plane's rotation. Restore `Orthogonal(normal)` and its spatial cross
  tangent in the planar branch of `ContactTangents` to reproduce the unchanged
  covariance failure. The repaired physical tangent is `normalize(n_plane ×
  n_contact)`; the absent second DOF has an exact zero Jacobian.
- `custom-inertia-before.log`: reading `(I^-1)zz` as `1/Izz` is incorrect for a
  non-diagonal custom physical tensor. Setting cached `planarInverseIz` directly
  to `inverseInertiaLocal[2][2]` reproduces the native angular-impulse failure.
  Restricting the physical quadratic form before inversion fixes it. Ordinary
  lamina tensors retain their direct path.
- `script-boundary-before.log`: a forbidden planar velocity raised a native C++
  exception across the JS boundary. It is now a catchable TypeError with unchanged
  physical state. Runtime pose/state checks also run before publishing records.
- `image-reuse-before.log`: the new editor image helper incorrectly retried the
  create-only Track API for an existing asset. The corrected helper reuses the
  existing indexed texture without replacing artwork or metadata.

These are failures found while promoting the prototype into production paths;
there is no claim that an already accepted native 2D release had these bugs.
Original Stage 0 experiments, Stage 1 evidence and fluid research stay untouched.
Early provisional builds/edits are retained locally; these logs do not claim to
be complete source snapshots of every intermediate edit.

## Harness/setup corrections (not engine failures)

An early player test advanced PhysicsWorld alone and omitted application
presentation publication. It was replaced with `StepAuthoritativeWorld`, the
actual played-world fixed path; no gameplay acceptance threshold was weakened.
One provisional corner oracle held a reference through an expired temporary;
its fixed, independent oracle still failed the original geometry and passed the
corrected clipping. A failed compile once left an older binary available; that
run was rejected, and subsequent build/run sequences use `set -e`.

The first application tour used the wrong WAIT directive; the corrected tour uses
`WAIT_SERVICES`. A save attempt outside writable roots was rerun with an isolated
`XDG_DATA_HOME`. A redundant manual runner reused fixtures whose tests correctly
require fresh output; those setup rejections are not classified as physics
regressions. Final native gates use fresh owned output paths. Detailed original
scratch logs remain under `.cache/2d-stage2-evidence`.

## Reproduction

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas_planar_physics_tests \
  judas_planar_integration_tests judas_editor judas judas_export \
  judas_planar_benchmark --parallel 2
python3 scripts/prealpha_validate.py --build-dir build --output /tmp/FRESH-2d-gates \
  --test planar-physics --test planar-integration
build/judas_planar_integration_tests /tmp/FRESH-2d-gates/planar-project --restore
build/judas_planar_benchmark
node scripts/check_judasjs_api.mjs
```

The benchmark includes sleeping, reports physical outcomes and is not an
all-active dense-impact guarantee. Raw host/compiler and RSS are included.
Application and editor checks use real SDL/OpenGL with Mesa llvmpipe; hardware
GPU performance and Windows/Android qualification are not inferred.

Full local application captures, editor theme/scale outputs and relocated package
are indexed in `.cache/2d-stage2-evidence/HANDOFF.json`. Read [the implementation
report and human checklist](../../2D_STAGE2.md) for support boundaries.

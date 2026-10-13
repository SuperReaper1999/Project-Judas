# M80 results — 2026-10-13

**Native Linux automated checks passed; operator human acceptance recorded on
2026-10-13** for implementation `0b19cda66e7dfc6d6d0232b936d435248d41c426`.
See [the acceptance record](HUMAN_VALIDATION.md); no implementation change or
validation rerun accompanies this status update. Starting accepted
Experimental/Stable checkpoint: `3745787d0732a05e82aba9496e592bc2233564f0`.
The operator authorized Stable promotion before M80; Stable already contained
that exact accepted M79 checkpoint, and the normal push was up to date. M80 was
first published to Experimental. After human testing the operator authorized
normal Stable promotion. Main remains at the branch-split checkpoint.

## Implementation and ownership

M80 reuses the existing M62 XPBD tetrahedral material, rate/capped isochoric
plastic rest updates, M63 cohesive interfaces and M79 section-strength model.
There is no new simulation pipeline, dependency, gameplay binding or decorative
debris path. The logical owner owns one nodal material population; current node
geometry drives material constraints, sampled physical contact, deformed-surface
ray queries and render bindings. Scripts apply ordinary forces/impulses and
choose interactions; they never set nodes, rest matrices or fracture state.

Soft structural interfaces now measure the accumulated cohesive reaction as
`f_i = -lambda_i / h²`, `F = sum(f_i)`, `M = sum(r_i cross f_i)` about their
current reference-area-weighted midpoint centroid. A volume-weighted polar
material frame converts resultants to M79's section frame. Strength failure
removes actual cohesion, exposes authored interior faces and retains node mass,
velocity and plastic shape. Interface strengths, family strengths and nominal
section moments remain ordinary generic material settings.

The normal inspector can bake a **bendable structural block**, edit material,
attachments and per-interface strength, and preview geometry. The existing
baker gains `flex-structure`; cook format remains the existing section version.
New saves use deformables participant version 3; retained streaming state uses
`Judas.DeformableState.1`. These preserve the original material frame as well as
the existing physical state. Legacy records still decode; missing historical
frames after an owner rotation cannot be reconstructed exactly.

See [implementation, equations and limits](../../M80_BENDING.md),
[reproduction](REPRODUCE.md) and [exact changed files](CHANGED_FILES.txt).

## Established defects and corrections

| Finding | Preserved failure | Generic correction and final result |
|---|---|---|
| Componentwise material response depended on global placement rotation | Accepted M79 loaded elastic rotation error 0.003683 m; plastic geometry 0.093362 m, rest 0.021879 m; supported oblique gravity 0.001757 m | Express elastic columns as `D_s inverse(D_p) R_initial`. Final elastic 8.94e-14 m; plastic geometry 2.49e-13 m/rest 7.45e-14 m; supported gravity 1.35e-10 m. No compliance/iteration retune. |
| Soft section capability was absent | Original cook rejected non-rigid structural cells; baseline capability controls fail | Reuse existing section geometry/strengths with soft cohesive force/moment resultants. Independent force, zero-net-force couple, stronger-interface and rotated controls pass. |
| Accepted nominal-area rounding polluted section centroid after translation | With 5e-7 area rounding and translation (10000, -20000, 30000) m, stress error 14.396492 Pa despite positional agreement 2.72e-10 m | Normalize derived centroid weights; accumulate relative to one midpoint. Same test: 2.39e-5 Pa, below unchanged 1e-4 Pa comparison. Strength area unchanged. |
| Historical constitutive frame missing in saves | One resumed step after owner rotation differed by 0.000124870483 m | Save/validate the original orthonormal material basis independently of current owner transform. Same 1e-12 m comparison: 0 m. Current stream and whole-world cold paths pass. |

Original fixtures/logs and tooling failures remain under `development/`. Eight
baseline failed assertions separate three pre-existing orientation defects from
five missing soft-section capability controls. An old-library/new-header ABI
attempt, a private-field test compile error, and missing transient TypeScript
tooling are classified as tooling errors. The first application cold comparison
ran before script initialization: capture correctly rejected it. The corrected
test completes the normal lifecycle with zero resumed fixed steps, compares full
participant bytes, then checks actual resumed physical motion. No lifecycle
restriction or acceptance threshold was weakened.

## Final-source Linux validation

All 17 cases in `linux/final-cases.json` exit 0 on the combined final engine.

| Scope | Result |
|---|---|
| M80 numerical | 40 checks: elastic recovery, plastic retention, mass/rest-volume, loaded rotation/oblique gravity, real deformed ray/contact, force/couple section failure, stronger strengths, exposed topology, stale-hit safety, independent physical continuation, legacy/malformed frame records |
| Existing deformable/fracture/structural/joint units | 66 / 33 / 32 / 29 checks pass |
| Current M80 application | Probe 7, cold write 9/read 8, streaming 11, radial 8; normal async resources, public JS/UI/camera/motor, actual press contact, prefab independence, destroy/reload and exact frame/shape retention |
| M79 structural application | Cold write 22/read 18; real rigid fragments, support/strength controls, mass, owned-joint continuation and actual resumed motion |
| M62 application | Probe 11, cold write 8/read 13, streaming 11, lifetime 12; existing cloth/bone attachment, plastic solid, presentation, prefab, residency and cleanup checks |
| Editor | New structural bake succeeds; existing destination rejects without changing assignment. Duplicate/undo and Play/Stop return authored state IDENTICAL; save succeeds, resource/job failures 0. Cook and screenshots retained. |
| Standalone/export | 14 assets, 3 scenes; normal launch, loads/fracture/prefab, pause/resume and flat→radial transition pass. Package moved to a path with spaces and launched from unrelated `/tmp`; cold write 9/read 8 pass against packaged content. |
| JudasJS declarations/drift | 345 symbols, 30 exports, 239 native operations, 16 callbacks, 39 examples, 3 negative drift checks; TypeScript 5.9.3 pass. Source/type checks, not live enumeration. No API addition. |

Release targets were rebuilt with `-O3 -DNDEBUG`. The final affected-target build
is warning-free. Earlier full core compilation records two pre-existing
misleading-indentation warnings in unchanged `RuntimeWorld.cpp`; one temporary
new test warning was corrected. This is a focused Release build/regression set,
not a new clean full-production-suite certification. The earlier
`structural-support.log` is retained separately and is not counted as a final
post-frame-correction case. No unrelated fluid/research suite was rerun.

Numerical measurements:

- Elastic fixture: 63 nodes/144 tets; displacement 0.0717974 m loaded →
  0.00563504 m after unloading.
- Plastic fixture: retained displacement 0.266063 m; rest-shape change
  0.0749330 m; maximum rest-volume error 8.33e-17, mass 100 kg unchanged.
- Soft section: peak tension 1159.07 Pa, topology revision 2, mass 30 kg
  unchanged. Pure couple: zero external net force, 80 Nm moment, peak 1035.33 Pa.
- Lab A loaded 0.281334 m → 0.0649481 m after unloading in gravity. Lab B
  retains 0.120674 m geometry/0.0195887 m rest change. Actual 2 kg rigid press
  dents D by 0.0955552 m with 0.0272717 m permanent rest change.

## Representative performance

Linux x86-64 Intel i7-4790, 8 logical CPUs. Fixed cadence 60 Hz. The lab uses
207 nodes, 384 tetrahedra and 9 ordinary rigid bodies at 4 substeps/4 iterations.
Samples: 300 frames after 20 warm-up frames. No quality/cadence cuts.

| Workload | Median ms | p95 ms | Maximum ms |
|---|---:|---:|---:|
| Flat lab total fixed-step | 3.736 | 4.849 | 6.950 |
| Radial lab total fixed-step | 3.795 | 5.033 | 11.977 |
| Flat application frame work, offscreen | 16.648 | 21.156 | 37.071 |
| Radial application frame work, offscreen | 15.325 | 19.583 | 33.156 |
| Eight simultaneously loaded 63-node/144-tet specimens, 4 substeps/6 iterations | 9.457 | 11.106 | 11.938 |

The eight-instance numerical batch measures per-instance solves without a world
inter-instance contact pass. Application measurements include the normal world.
Offscreen GL has no visible swap/pointer capture; these are CPU/app-work timings,
not displayed FPS or physical-GPU certification. The broader M62 regression lab
is heavier: captured frame p95 values are 60–72 ms with cold/startup maxima up
to 1.72 s. Those logs are preserved; passing correctness does not certify that
old lab's performance. No unrelated optimization was undertaken.

## Limits and outstanding acceptance

This is approximate finite-iteration XPBD, not industrial FEM or calibrated
engineering material. Section stress is corotated nominal reference-area stress,
not exact Cauchy stress on a warped interface. Plastic flow and brittle interface
failure are separate; there is no fatigue, arbitrary cutting or remeshing.
Inversion/nonfinite state stops diagnostically. Existing sampled soft contact can
miss small/fast geometry, has no exact soft CCD, general M44 soft casts,
CharacterMotor blocking or M42 soft-surface callbacks. Still-connected cohesive
parts suppress self-contact, so deep folds can self-intersect. Unit owner scale,
exclusive physical ownership and existing live-fracture residency pins remain.

The Windows VM was shut off; native Windows M80 validation is outstanding and
nonblocking. The operator accepted the Linux desktop demonstration; individual
checklist items are not assigned fabricated per-item results. See
[platform scope](PLATFORM.md) and [human checklist](HUMAN_CHECKLIST.md).

Protected historical evidence, independent fluid research, accepted fluid
implementation and existing game assets are unchanged. Current M62/M63/M79
reference documents only receive clearly identified M80 cross-links. Unrelated
pre-existing untracked artifacts remain outside this checkpoint. No M81 work,
tag or main update is included. Stable promotion follows the explicit subsequent
operator authorization recorded in `HUMAN_VALIDATION.md`.

`FINAL_SOURCE_SHA256.txt`, `BINARY_SHA256.txt` and `EVIDENCE_SHA256.txt` identify
the final candidate. Fingerprints exclude their own recursively defined hashes;
see the manifest notes in [reproduction](REPRODUCE.md).

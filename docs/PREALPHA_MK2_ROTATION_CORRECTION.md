# MK2 rotation and chronological impact correction

2026-10-24. Experimental candidate based on `bded63dfdaefdeec6484d66b3211712c200c2941`.
Automated Linux qualification is complete; **human acceptance is pending**.
Main is unchanged. This is Pre-Alpha engineering, not M100 or release qualification.

## What was wrong

This was a configuration-consistency defect across integration and contact response,
not excessive physical energy in the authored scene.

1. The old solver responded at a future pose, then rewound orientation while
   retaining angular velocity from that future configuration. For an anisotropic
   body, `E = 0.5 m |v|² + 0.5 ωᵀ R I_body Rᵀ ω`; changing `R` without a consistent
   change in angular state changes energy. The preserved first-impact trace has
   locally conservative response at 5077.33 J, followed by a rewind to 5781.02 J.
2. The rotation predictor solved an implicit midpoint/Cayley problem, but MK2
   advanced orientation using an exponential map of that generator. These are
   different finite rotations. Free asymmetric rotation alone gained energy.
3. Unbiased relaxation removed impulses using refreshed, rotated contact
   Jacobians instead of the configuration at which those impulses were applied.
4. Continuous response could combine a time-of-impact orientation with an endpoint
   angular velocity. Re-timing also conflated split position recovery with real
   velocity, and could query one participant's new path against the other's old path.

The earlier [investigation](PREALPHA_ELASTIC_LINUX_BASELINE.md), original fixtures,
failed local repairs and original records remain unchanged. Correcting one angular
transport operation was insufficient; the interval and contact ordering had to agree.

## Generic correction

### One rotation trajectory

`src/RigidRotation.cpp` constructs the actual dynamic trajectory used by both
integration and CCD. For asymmetric inertia, solve body-frame momentum midpoint

```
m_mid - m0 = (h/2) [m_mid × (I_body^-1 m_mid)]
Ω_mid = R0 I_body^-1 m_mid
R1 = Cayley(h [Ω_mid]×) R0
L_world = R0 m0
ω1 = (R1 I_body R1ᵀ)^-1 L_world
```

The midpoint equation preserves the quadratic free-rotation invariants in exact
arithmetic. The implementation uses double precision for the solve, then stores
ordinary float runtime states. It does not rescale energy or momentum. Analytic
principal-axis/isotropic rotation uses its exact exponential trajectory.

Step doubling bounds local orientation discrepancy to `1e-6` radians, independently
of the energy tests. Each midpoint solve has 12 Newton attempts; failed or inaccurate
leaves subdivide. There are at most 512 leaves per generated interval and depth 24;
unrepresentable intervals fail explicitly. Unchanged linear travel keeps a common
episode origin/time, avoiding accumulated position rounding at adaptive knots.
Geometry interval proofs use the corresponding curve; Cayley-only bounds do not
silently accept exponential leaves. Public motion sampling and coupled clock offsets
carry the same representation.

### Chronological contact response

The four-substep solver now relaxes impulses **at their own configuration**, separates
physical velocity from geometric position-recovery velocity, and advances through
actual arrival states. Compression and material restitution operate at that arrival
pose. Existing energy admissibility checks, frictionless retries and inelastic
fallbacks remain; no post-response energy clamp or damping was added.

Dynamic and kinematic arrivals share a chronological queue. Both affected paths
publish together before dependent queries run. Previously negative queries invalidate
when changed dynamic paths can overlap; committed history is never revisited.
Wet/dry liquids and other unrelated systems are not modified.

An arrival episode's allowed contact depth is chosen before adaptive knot traversal,
not restarted at each knot. Same-instant response iterations retain the global work
bound without consuming additional distinct future-arrival slots. Existing four
arrivals/body and 256/step bounds remain. A narrowly certified sphere/static-planar
fallback can preserve tangential travel while holding the unsafe normal motion;
an independent secondary obstacle blocks that travel through ordinary CCD.

Positive authored restitution does not always mean an actual bounce: the existing
0.5 m/s threshold is unchanged. A conservative point-speed bound determines when
speculative support must defer to timed restitution, including angular momentum's
inertia-dependent speed bound and rechecking after impulses. Quiet nonbouncing
supports retain normal speculative handling. This avoids a demonstrated settling
cost regression without changing the restitution law.

### Contact observations and waking

Chronological arrivals exposed repeated observations of the same support patch.
Coalescing requires matching body generations/revisions, nearly parallel normals
and the existing nearby-anchor tolerance. It sums **all** impulses and their pressure
centre; it does not increase the query budget, discard impulses or alter warm caches.
A focused comparison bounds observed reaction error to `1.20e-7` N s.

An actor genuinely awakened by CCD now joins the next solver boundary, retains its
stationary prefix and integrates the remaining time. The previous candidate missed
one final interval: the preserved late-wake endpoint was 3.290909 m instead of the
analytic 4.806061 m. Sleeping, already-awake and absent-target controls now pass.

## Independent numerical results

Energy is computed from authored cuboid mass/inertia and actual physical states,
not a solver-reported success flag. Collision tests independently examine paths,
finite features, containment and analytic impact time.

| Experiment | Original | Final candidate |
|---|---:|---:|
| Original retained executable, 5 m/s, 20 rad/s, e=1, 2 s | peak/start **1.31845052** | **1.00000020307**, zero holds |
| Original unchanged 100 m/s, 100 rad/s, one step | 5077.33 → **125043.475 J** | 5077.33 → **5077.33386 J**, zero holds |
| Free mixed spin, nominal rate 20, 2 s | inconsistent maps previously gained energy | max relative energy error **2.30e-6**, angular momentum error **5.56e-7** |
| Free mixed spin, nominal rate 100, 2 s | original defect | energy error **3.42e-6**, momentum error **7.65e-7** |
| Two free colliding spinning bodies | independent momentum control | linear error ≤**1.12e-6**, angular error ≤**1.67e-5** |
| Cold restore, saved during elastic motion | real snapshot and fresh process | all 13 physical scalars exactly equal for 240 subsequent steps |

The final envelope includes 256 combinations of restitution, speed, spin, rotated
frame and creation order, using the unchanged `peak <= initial * 1.0001` bound.
A further 128 oriented mixed-spin cases pass. An independently converged RK4 oracle
measures orientation error: approximately 0.0009905 rad at rate 20, 0.0078766 rad at
100 and **0.1209 rad at 1000** after two seconds. Conservation does not imply exact
high-spin phase; the latter remains a documented accuracy limit.

The original retained executable was rerun and reproduced the 31.845% gain exactly
(SHA in the evidence receipt). A separate fresh replay linked against the frozen
production archive gives 0.2654% for that moderate case, still failing the unchanged
0.01% bound; its one-step extreme failure reproduces exactly. The cause of this
multi-step cross-artifact difference was not established. Both raw records are
preserved; they are not presented as identical build artifacts.

### Remaining long-run dissipation

Nominal e=1 is **not** a guarantee of exact perpetual elasticity. At 60 s the ordinary
case loses 1.18% and mixed-spin case 0.815%, with no holds and no energy growth.
The pre-existing low-speed restitution cutoff explains much of this: a diagnostic-only
zero-cutoff build loses about 0.193% and 0.209% respectively. That alternate source
and results are preserved, **not installed**. Remaining discrete-contact dissipation
is not completely characterized. Extreme arrival caps can still truncate motion;
this work does not claim unrestricted perfect continuous dynamics.

## Validation actually run

- Release build: zero warnings in the final build log.
- 22 final-source native suites: rotation, MK2 foundation, continuation,
  diagnostics, joints, ragdolls, physical animation/balance, kinematics,
  CharacterMotor, collision geometry, impact robustness/timing, continuity,
  rigid motion/geometry/bounds, contact lifecycle, origin physics/assemblies,
  process-separated rotation save and original mesh continuation.
- New rotation suite: independent RK4/analytic controls, impulse accounting,
  restitution threshold, late wake, origin/coupled curves, momentum and energy.
  It reports 31,398 assertions; these are not 31,398 distinct engine capabilities.
- Curved-mesh fixture: six scenarios, original 90-step fast boxes, finite edges,
  sidedness/rotation and absent-collider controls; 372 checks pass. Original
  robustness suite: 4,412 checks pass, deepest corner approximately 1.83 cm.
- Genuine native application tests: M77 corrective action and cold restore,
  including restoring the exact snapshot from the failed intermediate candidate;
  Hearthworld tree/mining/gravity-gun/construction/excavation and fresh-process
  persistence; unchanged original JudasJS reproduction; actual export and launch
  after moving the package to an unrelated `/tmp` directory.
- Four validation-registration and 12 build-organization checks pass. Manifest
  includes both new native suites. No full production-suite marathon was run.

Application tests use X11/native NVIDIA rendering and dummy audio. They do not
constitute listening/audio quality, manual editor interaction, a clean-machine test
or human gameplay acceptance. Windows/Android are not qualified by this change.
The earlier Ubuntu 24.04 SDK must be rebuilt to incorporate this correction; its
previous compatibility evidence is not a test of these new binaries.

## Matched performance and memory

Ryzen 5 5600, 8 GB RAM, Linux, GNU 15.2, Release. Serial fresh-process ABBA order,
identical unchanged benchmark driver/settings: 30 warm-up +180 measured 60 Hz steps,
profiling enabled and **allocation interception disabled in both**. Shared desktop,
no rendering workload. All 16 runs pass the original physical acceptance conditions.
These are short representative workloads, not severe-disturbance or universal FPS
certification. Mean of each pair's median/p95, and maximum across its two runs, in ms:

| Workload | Median before → after | p95 before → after | Maximum before → after |
|---|---:|---:|---:|
| 128 pile | 0.079 → 0.091 | 1.047 → 1.190 | 1.436 → 1.557 |
| 1024 pile | 0.758 → 0.823 | 8.956 → 9.579 | 13.345 → 9.816 |
| 128 active mechanisms | 0.507 → 0.552 | 0.688 → 0.769 | 0.727 → 0.804 |
| 64 mesh bodies | 0.038 → 0.228 | 4.058 → 4.129 | 5.089 → 4.535 |

This is a correctness correction with measurable cost, **not a speedup claim**.
The original pile maximum varies 9.154–13.345 ms; the lower candidate maximum alone
is not evidence of improved worst-case architecture. Mesh has one remaining awake
body (63/64 asleep versus 64/64), explaining the larger quiet median; no artificial
sleep was introduced. All 1024 pile bodies sleep. Process peak RSS for that pile
is 78.45–78.68 MiB before and 78.59–78.82 MiB after. Motion-ledger capacity increases
655,360 →950,272 bytes (+288 KiB). Full RSS/profile/endpoint records are retained.

An earlier candidate made mesh p95 approximately 13.8 ms by unnecessarily deferring
subthreshold contacts. The conservative effective-restitution scheduling described
above removed that regression. Two earlier timing comparisons are explicitly
**rejected** because baseline allocation interception was enabled and candidate
interception disabled. Their raw records remain; only `performance-matched30` is
used for the table above.

## Evidence, failures and reproduction

[Evidence index](evidence/prealpha-mk2-rotation-correction/README.md) contains final
source/binary fingerprints, all final suite/application logs, raw energy traces,
matched benchmark archives, exact intermediate failed snapshot, and rejected
experiments. No executable, object or engine archive is committed as evidence.
The runtime identifies itself as a dirty precommit development build; the tested
source archive and per-file receipt bind its code without falsely relabelling it
as a clean commit build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2 --target judas_mk2_rotation_tests judas_mk2_rotation_cold_tests judas_mk2_diagnostics_tests judas_impact_mesh_cost_tests
./build/judas_mk2_rotation_tests
# Use a new output path; the writer refuses to overwrite an existing experiment.
./build/judas_mk2_rotation_cold_tests /tmp/judas-rotation-cold-new
./build/judas_mk2_diagnostics_tests
./build/judas_impact_mesh_cost_tests 60 '' 1.2
./build/judas_physics_scaling_benchmarks --workload pile --count 1024 --warm 30 --steps 180 --profile 1 --output /tmp/judas-rotation-pile-new
```

Benchmark target must also be built for the last command. Frozen-baseline replay
scripts record actual compile/link flags and archive hashes; reproducing that side
requires rebuilding the input revision or retaining the named local archive.
Intermediate patches omit some then-untracked helpers; available helper snapshots
are retained, but not every abandoned stage is advertised as a standalone build.

## Human acceptance checklist

1. Physics Stress Lab: ordinary stacks, scenario 3, thrown objects and moving camera;
   inspect support and collision behaviour, not just timing.
2. Balance Lab: use J for the corrective step, save/load in progress (F6/F7), inspect
   the actual landing; test passive/recovery and the rotated/radial fixture.
3. Hearthworld: fell/process a tree, mine, carry/launch an object, build and dig;
   save, quit completely and reopen to check physical continuation.
4. Optional original elastic reproduction: compare the automated energy trace with
   the moved standalone; visual plausibility alone cannot certify conservation.

No game rules, public JudasJS bindings, save-format identity, fluid implementation,
protected history, research, main branch or release channel were changed.

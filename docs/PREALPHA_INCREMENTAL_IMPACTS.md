# Pre-Alpha: persistent support and growing impact response

21 October 2026. Experimental implementation, **not human accepted**. Input:
`6059bafb43be4650162d9a6bdec05ea027135abc`; main remains
`3ecf26cdf248e271c96d17928bd33634e37f30ce`. No M100 or release tag.

## What was actually wrong

The previous dependency scheduler eliminated many unnecessary CCD requests, but
an arrival still discovered the whole touching island and ran another complete
inelastic projection. Ordinary resting contacts retain a small finite-iteration
velocity defect. Every new event tried to remove that unrelated defect again,
changing hundreds of velocities and invalidating their collision dependencies.
This was a contact-response ownership problem, not simply slow triangle tests.

The [original investigation](PREALPHA_IMPACT_RESPONSE_INVESTIGATION.md) and its
failure artifacts are preserved unchanged. An additional read-only observer
captures the **actual support ownership** at the original step 347. All 347 physical
step digests, work counts and velocities match the uninstrumented input engine.
Serializing cases inside step 347 makes its timing unsuitable for a speed claim.

| Exact original event | Bodies changed by old full solve | New increment | Negative control: forget continuing support |
|---|---:|---:|---:|
| 18 | 810 | 3 | 810 |
| 28 | 751 | 0 | 751 |

All ten captured old cases replay the original full-solve velocities exactly.
All ten new responses pass the existing energy policy. The zero-effect event
really has no new response; it must not restart the pile's resting solve.

## Physical model and ownership

Ordinary force integration, support warm starts, ten velocity sweeps, positional
correction, materials, CCD fidelity and 60 Hz cadence remain. **Dense trajectories
do change** because the event response and row discovery order change. This is
an explicit numerical-model correction, not a claim of bit-identical physics.

For a new/selected contact the normal target remains zero. For an existing pair
owned by the authoritative `supportPairs`, freeze its pre-event incoming normal
velocity `v_n^-` for the duration of this event. Its target is:

```
b = min(v_n^-, 0), if -min(v_n^-,0) * dt <= min(5 mm, existing quarter-feature allowance)
b = 0, otherwise

v+ = v- + M^-1 J^T lambda
lambda_n >= 0; J_n v+ >= b; |lambda_t| <= mu * lambda_n
```

These are incremental event impulses, not subtraction of two nonlinear
projections. Newly encountered contacts and the selected arrival are strict;
spinning contacts are not classified as continuing support. Large old defects
remain strict. The allowance is the **existing** physical support bound, not a
new tolerance. Incoming snapshots are immutable, including when a body first
appears in an appended contact. A partly solved velocity never becomes a new
reference. No cross-event momentum-history registry is added.

The ten-sweep solver is still approximate. Reject a private trial if its final
normal closure exceeds the same physical allowance or its energy check fails;
run the original complete-island path instead. The energy convention remains:

```
K_after <= K_before + work_by_prescribed_infinite_mass_surfaces
           + 64 * float_epsilon * max(1, K_before, abs(boundary_work))
```

This is the existing engineering tolerance, not a formal error theorem. Moving
and rotating prescribed boundaries use actual contact-point velocity and may
legitimately supply energy. Rejection never repairs/clamps the trial into a pass.

## Growing one event

`physics/WorldIncrementalImpact` owns private stable-address body copies, rows and
incoming snapshots. It queries actual seed contacts, then grows through bodies
whose represented velocity changes after **any individual row**. A transient
response cancelled by a later row is still discovered. An accumulator change
whose reaction rounds away at one endpoint creates no downstream velocity
change there: this uses exact equality, not an impulse or velocity cutoff.

Within each of the same ten sweeps, newly reached contacts are appended and
prepared at the same event pose. Existing rows retain their accumulated impulses;
they are neither restarted nor replayed. No island-size, propagation-depth or
small-impulse cap is imposed. A 100-body chain transmits a nonzero far-end response
and matches a prebuilt full ten-sweep chain exactly, including impulses.

Deferred wake-up gravity is applied once before the body's first snapshot.
Force-activated bodies have their contacts expanded before solving. No world
body, sleep flag or completed observation is published during a trial. Successful
closure publishes incoming poses for observations, then accepted velocities and
the existing motion/event invalidations. Failed trials discard their copies.

Ordinary resting-force guesses survive when neither endpoint velocity changed.
Changed endpoints invalidate those guesses. Impact impulses are never replayed
as warm support. Reused body generations cannot inherit an old occupant's load.
Completed-contact observations retain their existing group replacement policy.

### Scope and fallbacks

The new domain handles native box/sphere primitives without shape assets or pivot
offsets, including prescribed infinite-mass boundaries. Hard-jointed dynamic
participants, other geometry, external velocity coupling, ambiguous elastic
policy and failed quality checks retain the original full-island response.

There is no new component, public JudasJS binding, save schema or parallel world.
The scratch is event/step-local. Existing save, stream and origin ownership stays
with PhysicsWorld. `PhysicsWorld.cpp` remains the coordinator (867 lines); the new
private domain module is 139 lines. The shared contact-row arithmetic is retained.
An unused experimental sparse-row scheduler was **not** added to production.

## Matched Linux measurements

Ryzen 5 5600, 8 GB RAM, GCC 15.2.0, normal Release `-O3 -DNDEBUG`, same authored
`room1024.judas`, 1,024 loose pieces, 1,285 total bodies, 96 joints and 60 Hz.
Sequential workloads; no Windows VM/Android builds. Raw context records capture
actual environment and timestamps. `nvidia-smi` is unavailable on this host;
these are CPU physics measurements through native desktop GL, not a GPU claim.

Profiling disabled; synthetic one-step-per-frame runs establish step cost,
**not FPS**. Baseline is the exact 6059 source implementation (its frozen binary
was built from the byte-identical precommit candidate); source manifests explain
that provenance. Baseline measurements and binaries were retained before edits.

| Window | Mean ms old→new | Median ms old→new | p95 ms old→new | Worst ms old→new |
|---|---:|---:|---:|---:|
| first 270 steps |27.40→16.89|14.38→14.30|90.77→28.09|151.50→54.60|
| first 400 steps |67.40→20.06|31.30→18.53|201.88→36.46|535.21→81.33|
| all 600 steps |67.57→30.98|49.04→24.27|177.78→71.67|535.21→104.85|

All 600 CCD queries:4,745,707→2,506,219; search iterations:3,924,868→1,224,121;
events:5,682→4,955. Peak process RSS:177,920→177,012 KiB (not a meaningful memory
speedup claim). No swap. A second, independently run native-geometry observer
gets31.05 ms mean and the same600 physical/work rows. A profiled repeat also matches
all 600 rows and gets31.44 ms mean. These repeats establish reproducibility of the
candidate trajectory, not identity with the old chaotic pile.

### Real visible application

Identical35-second runs with actual frame cadence, profiling off, swap interval0
and the original eight-step catch-up policy:

| Observation | Old | New |
|---|---:|---:|
| late10–35s visible cadence |0.97 FPS|2.40 FPS|
| completed fixed steps |489|911|
| simulated seconds per wall second |0.232|0.431|
| worst fixed step |552.42 ms|124.47 ms|

**The requested24–30 FPS is not met.** The catch-up cap and discarded time remain
reported; no time-step/catch-up adjustment is used to manufacture smoothness.
These runs progress to different simulated times, so live timing alone is not a
matched physical-state comparison. The600-step runs provide that workload scope.

The final profiler's retained steps 483–600 still average roughly1,634 domain-body
visits across 4.85 accepted events per step, with some domains reaching721 bodies.
Real propagated velocity changes can still have large dependencies. In that tail
window, discovery averages11.82 ms, impact solving4.10 ms, ordinary continuation
cache1.05 ms and observation publication0.68 ms. CCD geometry/search are inclusive
categories and must not be added as independent disjoint costs. The growing solve
now excludes its nested domain-discovery time. Further contact discovery and CCD
work remains substantial; the root zero-effect re-projection correction does not
make arbitrary dense contact cheap.

## Correctness and qualification

- Normal Release build;25 focused suites pass: growing response, tracked rows,
  impacts/timing/robustness, support,406 collision-continuity checks,4,412 robustness
  checks, rigid contacts, joint/kinematic, CharacterMotor, ragdoll, continuation,
  contact lifecycle/cache, native geometry, event scheduling, origin/assembly,
  composed lifecycle, wheels, powertrains, structural and deformable physics.
- 50 growing-response assertions;300 deterministic full-versus-tracked row cases
  with exact body velocities/impulses. The latter is a correctness oracle, not a
  sparse-kernel performance claim.
- Four separately compiled broken mechanisms are detected: missing continuing
  support, rebaselining partly solved velocities, resetting impulses on growth,
  and missing transient activation. The corresponding unmodified control passes.
- Address/undefined-behaviour checks pass for the new solver tests and instrumented
  ContactSolver/ImpactSolver/RigidBody. Other archive modules remain Release.
  LeakSanitizer fails under this runner's ptrace restriction; its original failure
  is retained, and leak checking is explicitly **not** claimed.
- Fresh-process M70 and M77 saves pass. M77's actual restored endpoint exactly
  matches uninterrupted continuation and is physically standing. Original observer,
  controller, fixtures and thresholds are unchanged.
- Streamed save writer and two readers pass. Editor Play/Stop leaves its owned
  authored scene identical. Relocated standalone runs from unrelated `/tmp` with
  a read-only installation. Audio is dummy during automated checks; listening
  remains human review.
- Hearthworld's real shipped gameplay write/read checks pass (66+41 assertions):
  tree felling, mining, building/support loss, gravity manipulation, excavation,
  accounting, save capture and fresh-process restoration.
- 12 build-organization and4 validation-registration tests pass. New regressions
  are registered in ordinary collision gates. No full production-suite marathon.

### Native geometry at every end of step

The observer measures all 601 states, outside recorded fixed-step spans. The
baseline observer matches its original600 unobserved digests; the final observer
matches all 600 final unobserved digests and work counts.

Both retain all 1,024 bodies,2,048 kg mass, finite state and complete floor enclosure;
there are no gravity-contract mismatches or uncertain/unsupported native queries.
Scoped mechanical energy never exceeds its initial32,346.32J. Final energy is
23,899.81J old versus22,928.61J new; dissipation and dense trajectories differ.
This is the loose-piece population, **not a whole-world energy certificate**.

Penetration is mixed and remains a limitation: maximum72.27→74.34 mm,
p95 of each step's deepest penetration19.52→19.70 mm, final15.32→17.11 mm.
These measurements do not establish identical collision accuracy or zero
penetration. They do establish that speed was not obtained by removing bodies,
letting them escape, omitting CCD or inventing an easier fixture. Original
asserting regression thresholds are unchanged. Within-step transients beyond
end-of-step sampling are not universally certified.

## Rejected experiments and remaining limits

Preserved prototypes include global/historical contact references, an exact but
unhelpful sparse-row kernel and growing domains that restarted the whole trial.
The restart approach reached132.46 ms mean/1,022 ms worst over400 steps and was
rejected. The scalar historical-reference approaches did not solve ownership of
newly accepted resting state. Their sources, commands and measurements remain
alongside the adopted experiments; they are not represented as production.

Large real impulse domains and full legacy fallbacks remain costly. Float pose
quantization can still produce small contact arrivals. This change handles their
response consistently; it does not replace the float representation or certify
all collision features. Existing FLU-001, legacy-save, platform and unrelated
Pre-Alpha limitations are untouched. Windows/Android were not rerun on the8 GB
machine. Protected history, independent fluid research and game content are intact.

## Reproduce and review

See [the evidence index](evidence/prealpha-incremental-impacts-20261021/README.md)
for commands, raw CSVs, original cases, hashes, source/binary provenance, rejected
experiments and the portable original-event replay. The final-source inventory
and exact changed-file list live there. Build outputs/packages stay untracked.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas_growing_impact_tests judas_tracked_contact_rows_tests -j2
ctest --test-dir build -R 'prealpha.(growing-impact|tracked-contact-rows)$' --output-on-failure
```

### Human checklist

1. Open `projects/physics_stress_lab/physics_stress_lab.judasproj`; Play; press3.
   Watch the initial fall and late pile. Compare responsiveness, expecting slow
   frames; verify pieces still collide and remain inside the enclosure.
2. Return to1, pressF5 for the machinery tour, M to remove power, J/K to wake
   assemblies. Try4 for rotated geometry/gravity and P for real added bodies.
3. F6/F7 save/restore, R reload, Escape pause/resume; inspect contact behaviour.
4. Run the relocated export recorded in `application/PACKAGE.json`, with the same
   controls. Reopen Hearthworld and try gravity grabbing, felling and construction.

Experimental checkpoint eligible from the focused Linux results, **not** closure
of the live-performance finding. Main promotion remains the operator's decision.

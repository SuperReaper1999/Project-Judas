# Persistent-contact qualification preparation

Private planning/tooling only. No engine workload, build, test or production edit
was performed by this agent. The root agent owns the single serialized CPU lane.

## Frozen reference

`prepare_baselines.py` performed file-only verification on 2026-10-21 13:59:39 UTC:

- HEAD/branch: `f5624f0bb4f92ece7211199ca8fe51abc08a2b19`, Experimental.
- All 517 frozen source inputs match current source.
- Frozen/current normal `libjudas_engine.a` both SHA256
  `05f626d3fb5400bfd1db23ec37a3cd3e6490f8452145365662466eebccd705ec`.
- Original `room1024.judas` SHA256
  `a2caf62a295243d301149202c8a0a5eedafff256d68de0e3cda988846b2e05bf`.
- Exact copied raw 900-step, independent 900-repeat, 1800-step and live35 records,
  their logs/quality/frames, original core/source archive and low-overhead CPU-PC
  evidence are under `baseline/`. 37 files, 37,141,801 bytes; see `BASELINES.json`.
- Source mismatches on a later run mean candidate changes, not permission to
  regenerate baseline expectations. Copied targets refuse differing overwrites.

Normal reference timings (Release, original fixture, 60 Hz, ten velocity sweeps):

| Window | Mean ms | Median ms | p95 ms | Worst ms |
|---|---:|---:|---:|---:|
| Normal A, steps 1–900 | 37.7765 | 33.7417 | 77.5806 | 124.8949 |
| Normal B, steps 1–900 | 37.4679 | 33.4598 | 76.4475 | 124.6466 |
| Long normal, steps 901–1800 | 63.6237 | 60.5474 | 115.9294 | 297.7950 |

The live35 baseline has sampling enabled and reaches 895 steps: late 2.3539 FPS,
424.8208 ms/frame, eight steps/frame. Do not compare it as an unsampled control
without recording that distinction. Synthetic frames are never playable FPS.

## Prepared measurement commands (NOT executed)

Finish all builds and other game workloads first. Use fresh outputs, normal
Release physics and the unchanged fixture; keep the same driver between matched
runs. `run_candidate.py` prints a plan unless `--execute` is supplied. It never
builds, uses a process-group timeout only on its own workload, fingerprints source,
core, binary and fixture, isolates user-data writes, and rejects output reuse.
The advisory lock coordinates this runner only. Pass every intended untracked new
module/header with `--extra-source`; unrelated historical untracked files are not
automatically swept into its input inventory.

```sh
python3 .cache/persistent-contact-20261021/measure/run_candidate.py \
  --binary build/judas_physics_stress_live_benchmark \
  --mode 900 --output .cache/persistent-contact-20261021/current-900
# Inspect the plan, then repeat with --execute.
# Repeat sequentially with --mode 1800 and --mode live35, fresh output each time.

python3 .cache/persistent-contact-20261021/measure/compare_steps_frozen.py \
  .cache/persistent-contact-20261021/measure/baseline/unobserved900/steps.csv \
  .cache/persistent-contact-20261021/current-900/live/steps.csv \
  --start 1 --end 900 --output .cache/persistent-contact-20261021/current-900/parity.json
```

For a representation-only change require exact state/work parity. For an approved
solver/initial-guess change, physical trajectories may genuinely differ. Preserve
the mismatches; they do not automatically prove failure or correctness. Compare
native endpoint/all-step penetration, mass, containment, finite state, energy and
momentum on the same fixture, plus asserting subsystem oracles. Never declare
changed work counters a regression simply because fewer queries were performed.
No frame-cap, object-count, cadence, CCD or iteration-budget adjustments are allowed.

## Focused regression targets, derived from current CMake

Build sequentially/small parallelism; run each executable separately with its log,
binary hash, command and exit status. These names are actual targets, not proposals.

First-tier mechanism checks:

```
judas_growing_impact_tests
judas_tracked_contact_rows_tests
judas_impact_solver_tests
judas_impact_timing_tests
judas_impact_robustness_tests
judas_support_continuation_tests
judas_collision_continuity_tests
judas_contact_lifecycle_tests
judas_contact_cache_tests
judas_contact_storage_tests
judas_impact_candidate_cache_tests
judas_impact_event_schedule_tests
judas_rigid_contact_tests
judas_native_mesh_growth_tests
judas_collision_geometry_tests
```

Critical interactions on the combined candidate:

```
judas_joint_tests
judas_kinematic_tests
judas_character_motor_tests
judas_ragdoll_tests
judas_physics_continuation_tests
judas_origin_physics_tests
judas_origin_assembly_tests
judas_prealpha_lifecycle_tests
judas_save_streaming_tests
judas_physical_animation_tests
judas_physical_balance_foundation_tests
judas_physical_balance_controller_tests
judas_character_lab_tests
judas_balance_lab_tests
judas_wheel_tests
judas_powertrain_tests
judas_structural_tests
judas_bending_tests
```

Use the existing portable original-event replay unchanged:

```sh
python3 docs/evidence/prealpha-incremental-impacts-20261021/replay.py \
  --build build --output .cache/persistent-contact-20261021/original-event-replay
```

### Cold M70/M77, actual physical endpoints

The original harness is `.cache/contact-redesign-20261021/cold/validate.py`. Do not
run it in place: its original output is preserved and it requires a fresh directory.
`prepare_cold.py` copies it to fresh output, changing only ROOT/BUILD wiring. The
protected observer remains byte-exact SHA256
`715dd4ae56bc5a8b9556379d95bb5b6e8f4402fd4a60515b69ba039f0b918e13`.

```sh
python3 .cache/persistent-contact-20261021/measure/prepare_cold.py \
  --output .cache/persistent-contact-20261021/cold-current --build build
python3 .cache/persistent-contact-20261021/cold-current/validate.py prepare
python3 .cache/persistent-contact-20261021/cold-current/validate.py run
```

The last command compiles only the unchanged private endpoint observer and runs:

```
build/judas_character_lab_tests OUTPUT/character cold-write
build/judas_character_lab_tests OUTPUT/character cold-read
current-standing-observer OUTPUT/balance cold-write
current-standing-observer OUTPUT/balance cold-read
```

It compares actual uninterrupted/restored M77 physical endpoints and standing
state, save-byte preservation, and source/archive/binary fingerprints. It does not
accept only a restored controller status. Original timestep/settings/gates remain.

Real consumer gameplay, using existing public fixture/arguments:

```
build/judas_hearthworld_application_tests projects/hearthworld/hearthworld.judasproj write OUTPUT/game
build/judas_hearthworld_application_tests projects/hearthworld/hearthworld.judasproj read OUTPUT/game
```

Use dummy audio for automation and native desktop GL; audible quality remains human.
Existing `.cache/contact-redesign-20261021/application_checks.py` and `final_export.py`
show the established Play/Stop, streaming/save and relocated/read-only export path.
Adapt only fresh output roots; never overwrite their original evidence.

## Ownership/behaviour tests to add once implementation interfaces exist

Each check needs a physical observation and an intentionally broken private
variant proving it can fail. Cache hit counts alone are not physical validation.

| Case | Positive / physical oracle | Negative mechanism that must fail |
|---|---|---|
| Unchanged support graph | Resting stack, repeated actual impacts, independently enclosed native geometry; unchanged fixture/10 sweeps; compare drift/energy | Clear all retained support estimates every event (causal baseline, not automatically a correctness failure) |
| Independent assemblies/shared static floor | Impulse one assembly; untouched assembly follows independent reference | Treat the shared static floor as a dynamic propagation hub |
| Destroy/reuse body slot | Warm stack, destroy support, recreate different mass far away in reused slot; original supported body falls; new body gets no inherited impulse | Ignore body generation when reusing persistent edges |
| Filter/pair disable | Warm support; SetCollisionFilter or SetPairCollisionEnabled disables it; body falls with no support reaction, then cleanly re-enable in separate controlled fixture | Reuse contact edge despite changed collision filter/pair eligibility |
| Sensor/query/disabled state | SetBodySensor/SetBodyEnabled transitions remove physical response while proper events/query behaviour remains | Cached response bypasses current classification |
| Geometry/finite feature | Move/reset/rotate support; native contact witnesses/normals change; finite edge departure becomes real flight; curved-wall fixtures unchanged | Reuse stale witness/normal after edge/side invalidation |
| Material/mass/inertia | SetPhysicalMaterial friction/restitution or SetMassDistribution; current equations/limits, no old-authority reaction | Retain prepared effective mass/friction/restitution after its revision changes |
| Joint/authority handoff | Existing SetMotionType/joint negative fixtures reject or invalidate properly; fallback remains coherent | Permit free-primitive continuation through a hard-jointed/coupled participant |
| Own impulse dependency | Exact row cases with zero represented body delta but nonzero accumulator change | Queue only endpoint velocity changes and ignore own accumulator revision |
| Transient propagation | Existing original-event and growing-chain cases transmit through bodies whose net final velocity returns to its start | Expand only finally changed bodies |
| Warm support versus impact | Separating and returning bodies do not replay an old impact as a resting load; actual completed observations remain actual | Copy current event impulses into ordinary next-step warm cache |
| Reload/stream/cold save | New RuntimeWorld or restored region rebuilds valid owned data; M70/M77 real endpoints match uninterrupted continuation | Retain pointers/slot identity across destroyed World or serialize unowned scratch |
| Origin change | ShiftOrigin keeps local witnesses and velocity/momentum; existing origin/assembly fixtures | Store world witness without translating/revalidating it |
| Rotated gravity | Rotate complete analytic stack/impact fixture; transform results back and compare invariant oracle | World-Y-specific support or force test |

### Narrow warm-estimate control: targeted adversarial fixtures

The private `support-control` preserves ordinary old estimates, not current event
impulses. Current normal/anchor/generation matching and fresh effective masses are
still applied; this is a defensible causal experiment. With only ten sweeps it is
NOT guaranteed harmless merely because a warm impulse is called an initial guess.

1. **Separating contact:** warm a box under gravity, give it a genuine upward or
   sideways separating impact, observe release and short ballistic return. No
   adhesion/extra launch; actual native contact truth and energy are the oracle.
2. **Tiny impact:** stable multi-box stack plus tiny real sphere impact; record
   finite motion, native penetration and physical drift. Compare cleared versus
   retained guesses; do not demand unchanged chaotic trajectory.
3. **Huge impact:** same stack, strong impact deliberately unloads/reverses supports;
   track bounded passive-system energy/momentum and no stale force after separation.
4. **Mass/inertia change:** after warming, decrease/increase mass and anisotropic
   inertia via SetMassDistribution; use an independently fresh cold-guess reference
   at that instant and analytic single-contact result where possible. Existing
   SetMassDistribution does not clear contactCache, unlike SetPhysicalMaterial.
5. **Elastic return:** coefficient of restitution positive, isolated support and
   short return; enforce original bounce and fallback policy. Broken variant that
   routes elastic flight through inelastic support retention must be detected.
6. **Observation ownership:** ReadBodyContactSample and touch-event impulse must
   equal the actual completed solve; retaining a numerical guess must not report
   that guess as this event's physical impulse.

## Avoid repeating rejected approaches

Read `docs/evidence/prealpha-incremental-impacts-20261021/prototypes/` and
`.cache/contact-redesign-20261021/NEXT_IMPLEMENTATION.md` as dated experiments.

- **world-v3 sparse rows:** CSR dynamic adjacency and ordered active heaps retained
  exact sweep order and passed 300 randomized row cases; visits fell
  878,345→502,791, but the 400-step world took 63.964 ms mean versus 61.607 ms for
  v2 with exact physical/work parity. Fewer row visits did not imply less CPU.
  Preserve own-accumulator and transient endpoint dependencies; do not repeat an
  active-heap kernel alone and claim architectural closure.
- **world-v4 restarted domain solve:** restarting every growing trial reached
  132.455 ms mean / 647.612 ms p95 / 1022.256 ms worst over 400 steps. Rejected;
  retain already applied event impulses when appending rows at ONE physical instant.
- **world-v1/v2 scalar historical references:** did not correctly settle ownership
  of newly accepted resting state; mixed penetration outcomes. Not valid evidence
  that a scalar historical velocity/residual reference fixes the current graph.
- Existing compiled negatives in `negative-final/RESULTS.json` detect missing
  continuing support, rebaselining partly solved velocities, resetting impulses on
  growth, and omitting transient activation. Their original failed logs remain.
- General exact memoization has tiny measured WITHIN-STEP repeat rates. Reusing
  a Boolean miss on changed trajectories needs a conservative certificate; current
  reports do not measure complete cross-step memoization hit rates.

No physics budgets, object counts, controller settings, timestep, thresholds,
sleep rules, FLU-001, protected history or independent fluid research are altered.

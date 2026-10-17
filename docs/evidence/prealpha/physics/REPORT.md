# Pre-Alpha collision investigation: corrected evidence, rejected runtime repairs

## Shipping decision

**No collision/CCD runtime correction ships from this investigation.**
`PhysicsWorld.cpp`, `CollisionGeometry.cpp` and `CollisionGeometry.h` were restored
byte-for-byte to accepted `9cf491a13dbb1cd82ab9ab9b916e255af2d579d2` after the
candidate corrections failed matched performance comparisons. They must not be
presented as corrected production physics. The shipping change is an honest,
asserting test/oracle with independent controls. Curved-mesh continuation remains
a release blocker. No fluid, solver budget, timestep, fixture, material or
acceptance threshold changed.

## Original failure and oracle

The original Release executable reproduces `fastBoxesThrough=24` for each of the
104-, 1,800- and 7,200-triangle troughs, but reports 156 checks and zero failures
when invoked with a one-step ragdoll prefix. These fast-box trials independently
run their original 90 steps. First-crossing traces show genuine interior crossings:
for example, an approaching box ends step 1 at approximately
`(-1.10, -0.14, -0.80)`, still moving `(0, -40.33, 6)` m/s, with no impact event.
The center is well below the lower surface, not above the trough or beyond its lip.

The old radial-distance oracle also has a separate mathematical false positive:
it measures distance outside the *entire* circle and can call sufficiently high
legal flight penetration. The new `SheetOracle` interpolates the actual cooked
triangle rows, measuring signed depth along the local triangle's unit normal.
It retains the original **5 cm** threshold, population, inputs and 60 Hz timestep.

Controls check upper flight, genuine lower-surface penetration, finite extent,
absence of the collider, one-sided rear entry and arbitrarily rotated geometry.
The `entry` argument separates the initial two-step arrival contract from the
unchanged default 90-step glancing/continuation contract; it does not replace it.

On the final unchanged physics, the new tests honestly report:

| Contract | Coarse / medium / dense crossings | Result |
|---|---|---|
| Initial two-step arrival | 20 / 16 / 16 | 193 checks, 4 failures including rotated coarse |
| Full 90-step motion | 24 / 24 / 24 | 193 checks, 4 failures including rotated coarse |
| Flat initial arrival | 0 / 0 | 128 checks, no failures |

A test executable no longer silently reports success while these curved boxes cross.

## Established mechanism and remaining uncertainty

`ImpactTime` uses a proximity/response manifold of a concave mesh as though its
selected local plane separated the entire mesh. A trace of the 6 m/s tangential
case shows the selected gap changing from 0.02256 m to 0.13279 m while the response
normal changes to the tangential Z axis. Dividing that gap by 6 m/s advances past
the actual surface crossing. A selected patch's separating plane is not a
separating plane for a union of finite patches. The M44 cast implementation
already distinguishes that geometry, but the rigid-body CCD path does not.

For convex finite patches, any unit-axis projection gap is a lower bound on
Euclidean separation. With patch distances `d_i`, the union distance is
`min_i d_i`. A conservative scalar distance bound can be advanced using
`L = |v_A-v_B| + |omega_A| R_A + |omega_B| R_B`, rather than the closing speed
along an unrelated local response normal. One-sided feature selection and
already-touching continuation remain additional restrictions; none of these
experiments establishes universal continuous CCD.

Later glancing traces show centers entering the back side of one-sided local
faces, after which center-based sidedness excludes future contacts. Cumulative
penetration-target/support continuation is implicated, but a complete generic,
practical replacement was **not** established. Do not call that mechanism fixed
or invent historical continuation state.

## Rejected alternatives

All candidates retain the original contact solver, budgets and physical inputs.
Patches and exact source/binary identities are preserved, not committed as runtime.

| Experiment | Correctness observation | Measured cost / decision |
|---|---|---|
| Full per-patch CCD and continuation | Initial arrival passes; full trial still crosses 4 / 4 / 0 | 256-body ordinary mesh p95 about 2 ms → 57–60 ms; 1,024-body p95 9 ms → 210 ms. **Rejected.** Remaining pathological measurements stopped explicitly. |
| Per-patch separated arrival; original support continuation | Initial arrival passes all 193 checks; full trial 11 / 20 / 16 | 60-step jointed coarse trough total 298.22 → 458.71 ms, worst 17.44 → 32.48 ms; dense total 832.36 → 1,127.28 ms, worst 43.98 → 58.06 ms. **Rejected.** |
| Existing SAT/segment distance plus scalar-speed advancement | Initial arrival passes all 193 checks; full trial still 11 / 20 / 16 | Coarse total 304.41 → 477.59 ms, worst 18.36 → 28.19 ms; dense total 828.97 → 1,490.66 ms, worst 44.14 → 78.25 ms. **Rejected.** |

The 60-step ragdoll comparison is a measured prefix, not a full 300-step result.
Each executable also runs its deterministic repeat and separate fast-box trials;
reported `totalMs`/`worstStepMs` describe the first ragdoll pass only. Exit 1 in
these receipts is the now-asserted fast-box failure, not a fabricated timing pass.

Two reversed-order 256/1,024-body matrices cover free flight, independent stacks,
ordinary mesh and sleeping constrained machinery. The narrower experiments pass
all original physical gates and preserve bit-exact trajectory digests in these
fixtures. That does **not** excuse their curved-ragdoll cost or certify the much
more strongly connected M91 dense room. Small timing differences are not claimed
as engine-wide improvements. No production speedup is claimed.

## Reproduction and provenance

From a Release build, ordinary source test target:

```
cmake --build build --target judas_impact_mesh_cost_tests -j2
build/judas_impact_mesh_cost_tests 1 flat 1.2 entry
build/judas_impact_mesh_cost_tests 1 trough 1.2 entry
build/judas_impact_mesh_cost_tests 1 trough
```

The latter two commands **must fail** on the unchanged final physics. `1` limits
only the ragdoll prefix; fast-box duration is printed separately. Use 60 or 300
for longer ragdoll observations. Known failures must remain explicit in release
reports; do not reinterpret them as accepted correctness.

`MATCHED_FULL_PATCH_BUILD.json`, `MATCHED_INITIAL_PATCH_BUILD.json` and
`SCALAR_BUILD.json` describe the rejected binary variants. The accepted archive
replaces the three physics objects with exact accepted sources/original headers;
common support libraries and the same driver objects are used on both sides.
The scalar variant replaces only `PhysicsWorld`. Binary/source hashes and actual
commands are recorded. Archived `.py.txt` files are execution records with the
original scratch paths; copy/adapt them into a **fresh isolated scratch/output**
location before reproducing. Do not run them over retained evidence.

Original traces and failed intermediate variants are under `raw/` and
`experiments/`; preserved old evidence is untouched. A cached application-object
attempt at the dense-room diagnostic failed at startup before any physics step;
it is recorded separately and is not attributed to the physics candidate.

## Next bounded engineering boundary

A future correction needs a reusable concave-query/continuation representation
that preserves finite patch identity and one-sided contact validity without
restarting a complete TOI solve for every triangle. Profile and reuse derived
geometry carefully; demonstrate both fast thin-body correctness and curved
articulation cost before shipping. Do not trade one failure for a major normal
workload regression or increase budgets until the test happens to pass.

## Current extreme dense-room diagnostic

The fresh current application ran the exact registered `room1024.judas` fixture
(SHA-256 `a2caf62a295243d301149202c8a0a5eedafff256d68de0e3cda988846b2e05bf`),
with the original 270 requested fixed steps, synthetic 60 Hz input cadence,
profiler enabled, tour disabled and hidden rendering. **All 270 fixed steps,
endpoint quality observation and a valid profiler export completed.** The outer
60-second watchdog subsequently expired during application teardown; this is
not recorded as a successful process exit. No unfinished-step cost is invented.

- Fixed-step median **31.560 ms**, p95 **481.568 ms**, maximum **2,119.208 ms**.
- Worst step: **51 impacts**, **501,599 TOI queries**, peak island **973 bodies /
  2,517 contact rows**; total population 1,285 bodies.
- Inclusive work: geometry **933.545 ms**, island discovery **723.642 ms**,
  TOI search **327.743 ms**, impact solve **167.183 ms**. These categories overlap:
  **do not add them**. Continuous impacts occupy about 2,094 ms; main-view render
  submission is about 87 ms outside that fixed-step span.
- Worst-frame observed process RSS **375.6 MB**, including rendering, assets and
  profiler storage; profiler reserved estimate **70.8 MB**. Neither is GPU memory.
- Endpoint deepest observed penetration **7.217 mm**, no uncertain/unsupported
  queried primitive pairs and no nonfinite fields.
- Every one of the **270 trajectory digests and physical/collision counters**
  matches the preserved M91 270-step receipt, including final digest
  `192f0f4bed728953`. Thus the strongly connected event cascade is reproduced,
  not repaired. The timing difference from the historical run is not attributed
  to an algorithm change: final physics source is unchanged.

See [the current diagnostic receipt](dense-current-fresh/RESULT.json) and
[compressed profiler](dense-current-fresh/run/profile-shutdown.json.gz).
The earlier cached-object startup failure remains in `dense-current/` separately.
The dominant remaining cost is repeated connected-island discovery/geometry and
incident TOI rescheduling, not GPU work or a demonstrated need to lower fidelity.

## Evidence layout and review

- [Investigation classification](INVESTIGATION.json).
- [Original first-crossing trace](raw/first-crossings.log).
- [Final unchanged entry failure](raw/final-unchanged-entry.log),
  [full failure](raw/final-unchanged-full.log), and
  [flat positive control](raw/final-unchanged-flat.log).
- Matched data: `measurements/` (rejected full-patch continuation),
  `measurements-narrow/`, `measurements-scalar/`.
- [Exact raw compression identities](COMPRESSION.json): `.json.gz` files preserve
  their original decompressed bytes. No binaries or build outputs are included.

Human review should confirm known curved cases appear as failures, flat controls
remain usable, and no report calls these partial research repairs production
fixes or claims that the 1,024-body connected room meets 60 Hz.

# M78 corrective continuation — combined review candidate

Starting HEAD remains `9e77b2226edcf217ab789fe4f0fea42a3415a72d`.
**Uncommitted, unstaged; human review pending.** M77 remains checkpointed but
not human validated. This is a continuation of its corrective investigation,
not a new milestone. The [frozen audit](M78.md) and
[first corrective handoff](M78_FOLLOWUP.md) remain historical evidence.

## M77 regression: mechanism isolation

The original Linux and Windows failures, original 60 Hz timestep, 29 N s
impulse, rigs, controller, gains and acceptance thresholds are preserved.
Isolated source/header copies and engine archives were used to change one
mechanism at a time. A unit pass alone was rejected when the actual public-JS
application still failed.

Three generic seams were corrected:

1. **Completed observations versus warm-start ownership.** TOI processing
   invalidated articulated warm-start history, also erasing completed stance
   loads; impact rows were not published to the observation source. A separate
   completed-step ledger retains actual start/TOI solved rows. Only re-solved
   primitive groups are replaced. Existing generation, revision, filtering and
   bounded sample rules still apply; no replayed impulse becomes a new load.
2. **Event angular state.** Dynamic samples must transport angular velocity to
   their sampled orientation. A new explicit right-limit evaluator selects
   post-event motion at shared times; existing left-limit evaluation remains.
   CCD bounds use the geometric drift generator, not transported endpoint omega.
3. **Euler-top integration.** The explicit predictor preserved world momentum
   after transport but changed free-drift kinetic energy. A bounded implicit
   midpoint momentum solve supplies the existing Cayley/quaternion drift.
   Full-horizon free angular momentum and quadratic energy are invariant up to
   Newton/storage error. Nonconvergence is explicit; no dissipative fallback.

No `PhysicalBalance.cpp`, M77 authored content, script, fixture or acceptance
criterion changed. Removing gyroscopic transport, changing a foot reference
and using an RK4 endpoint were diagnostic alternatives, not retained fixes.

| Unchanged case | First candidate | Combined Linux | Native Windows VM |
| --- | ---: | ---: | ---: |
| Controller fixture | 119 checks / 2 failures | **119 / 0** | **119 / 0** |
| Actual Balance Lab corrective step | 15 / 1 | **15 / 0** | **15 / 0** |
| Motion, including independent orientation convergence | Original receipts retained | **10,239 passing checks** | Passing native execution |
| Contact lifetime / actual TOI observation | Original fixtures retained | **1,076 / 0** | **1,076 / 0** |

[Isolation table](evidence/m78-continuation/M77_MECHANISM_ISOLATION.md),
[equations, independent arithmetic and actual consumed-segment proof](evidence/m78-continuation/M77_ANGULAR_MIDPOINT_PROOF.md).
The new M78 energy check is tighter, not relaxed: `1e-4` instead of `5e-3`.
At the roundoff floor, convergence is measured against an independent refined
orientation solution rather than requiring energy rounding error to decrease.
No original M77 check was changed.

**Numerical limit:** shortened anchored CCD segments are not exact midpoint
endpoints. Their energy interpolation error converges with refinement but is
not eliminated. The independent reviewer records same-state examples; lower
aggregate error on the passing trajectory is not proof of eliminating that
error. Force/torque kicks, impacts and constrained integration remain approximate.
Singular/locked-axis inertia retains its existing separate path.

## Four-ragdoll tail: measured work and correction

Original workload: original 13-body rig copied four times, 52 articulated bodies
plus a floor, 48 joints, 8,192 triangles, 900 steps at 60 Hz. Geometry, cadence,
contact fidelity, CCD requirements and thresholds are unchanged.

The original slow collision step spent 32.796 of 34.207 ms in continuous
impacts. Narrow instrumentation attributed about 85% of continuous-impact time
(about 81% of the whole step) to geometry
queries that constructed full manifolds even when the caller needed a Boolean.
The shared SAT/clipping/witness traversal now stops at the first *accepted*
witness for those existence consumers. Response/proximity still constructs
complete manifolds. No bounds, iterations, search policy or collisions were
removed. Sphere/capsule fallback remains unchanged.

Independent parity: **43,980 samples / 286,091 checks**, all full-manifold
fields bitwise equal to the preserved original oracle; Boolean answers agree.

| Same four-rig workload, Linux | Median ms | p95 ms | Maximum ms | Above 16.67 ms |
| --- | ---: | ---: | ---: | ---: |
| First failed corrective candidate | 1.040693 | 18.875046 | 33.793832 | 78 / 900 |
| Boolean correction alone | 1.032763 | 14.963051 | 23.999817 | 30 / 900 |
| Combined final midpoint candidate | 1.058026 | **5.498837** | **26.653463** | **11 / 900** |

Native Windows VM, same final four-rig fixture: median **1.8522 ms**, p95
**10.839 ms**, maximum **56.376 ms**, **37/900** steps above 16.67 ms.
The median/p95 targets pass; the maximum exceeds the unchanged 50 ms comparison
target. That target was defined for the isolated development host; the VM result
is reported without weakening it or claiming hardware performance acceptance.
The maximum spends 54.2711 ms in the solver, with the same 285 CCD queries,
830 iterations and one bounded limit. Native p95-tail solver share is 91.593%.
All aggregate physical counters and the 26/52 sleeping result match Linux.
These VM timings are a separate platform observation, not a comparable
before/after optimization claim.

The existing p95 gate passes. The combined physical trajectory changes, so only
the paired Boolean isolation establishes that optimization's independent effect.
Remaining slow steps are dominated by repeated CCD/contact/joint work: 90.78%
of p95-tail wall time is in the solver timer; worst step has 285 CCD queries and
830 search iterations. Late ordinary operation is dominated by cooked contacts.
The retained final profile does not contain the early maximum's subphase trace;
no more precise final decomposition is invented.

Final mesh-4: 56,576 contact rows, 19,163 CCD iterations, seven bounded search
limits, minimum bone-center Y 0.0447216108 m, **26/52 sleeping** after 15 seconds.
Mesh-1 has 0/13 sleeping; both box controls have all bodies asleep. The unchanged
finite/penetration guard passes. Not all mesh rigs settle; no forced sleep,
extra damping or fixture simplification was used.

[Final timing, remaining costs and all raw traces](evidence/m78-continuation/final-performance/README.md).

## Combined affected validation

Linux affected Release rebuild and final source-linked probes pass. The first
build attempts with a missing include and an incorrect target name are retained
as tooling/build failures, followed by corrected receipts. No full production
suite was repeated for this narrow continuation.

- 50-case Linux matrix: 49 expected passes and the deliberately retained
  **FLU-001 failure**. Separate controller, motion, actual step and complete
  geometry parity checks also pass.
- M70 physical animation, joints, ragdolls, contacts/cache/storage, all 89 impact
  timing/CCD cases, kinematics, CharacterMotor, gravity selection and original
  FTFT3 gravity fixtures pass.
- Actual M77 variants, recovery, radial, lifecycle, region ownership, negative
  controls and three Linux-generated separate-process save pairs pass. M70 application/cold saves,
  kinematic application/save/stream and mixed lifecycle checks pass.
- Retained prefab, streaming, scalability and navigation transaction corrections
  pass on the same final source. Owned test copies contain intentional changes;
  original projects and published kits do not.
- Editor duplicate/undo/Edit/Play/Stop leaves the authored scene **IDENTICAL**.
  Offscreen SDL cannot validate interactive pointer capture.
- Fresh four-scene Balance Lab export moved to `/tmp`, launched from unrelated
  cwd with unavailable engine-root override, passes scene switches, save/load,
  pause/resume and reload. It is a normal project/package, not an engine mode.

Native MSVC combined validation includes the same physics/contact/CCD/gravity,
M70/M77 application, save, lifecycle and retained correction coverage; see the
[native matrix](evidence/m78-continuation/WINDOWS_FINAL_MATRIX.json) and
[native report](evidence/m78-continuation/WINDOWS_FINAL_REPORT.md).
There are **49 focused executions: 48 program passes and one cold-save failure**;
the native maximum-time target failure is additional. The verified existing
archive plus three affected translation units and fresh test/app links were
used; this is not a new whole-engine clean Windows build. Native compilation
warnings remain in the receipts.
Standing/recovery cold saves pass, but the in-progress-step cold save fails;
it is detailed below. A fresh native four-scene package passes startup, scene
switching, save/load, pause/resume and reload after moving and launching from
`C:\Windows\Temp` with an unavailable engine-root override. The initial VM
graphics-driver launch and missing test-fixture failures remain preserved;
the same executable/fixture succeeds after the documented environment/input
corrections. Software-rendered VM execution does not establish physical GPU,
controller, audio or human Windows acceptance. No full production suite rerun.

## Retained failure and deferred work

**FLU-001 remains failed and unchanged:** 731 checks / one failure;
0.558701396 m/s versus the original 0.5 m/s-at-four-seconds check. Its conditional
continuum calculation and proposed later settling criterion remain
[recorded for review](evidence/m78-followup/FLU-001.md). No fluid implementation,
research, fixture or threshold changed. M54/M55 conservation is not recertified
by this angular/contact correction.

The unchanged native fluid case passes at **0.419123590 m/s** (731/0), while
Linux still fails. This platform difference does not close FLU-001 or authorize
a fluid correction; both outcomes are retained.

**An additional in-progress cold-save failure remains open:** native 13/1;
the exact byte-preserved native snapshot also fails on Linux (13/1), although
that same native live state continued uninterrupted stands. Seven dynamic
body states, joint settings and controller history survive exactly. Actual
angular-drive command replay alone and a first-sample waiting bypass both
fail; neither is retained. A one-step warm-seed isolation further demonstrates
joint initial-guess sensitivity without changing geometry, contacts, inputs or
commands. No sufficient generic cold-continuation repair is established; no
controller/persistence change, cache serialization, iteration increase or test
retuning was added. [Exact reproduction, failed alternatives and diagnosis](evidence/m78-continuation/M77_COLD_CONTINUATION_DIAGNOSIS.md).

Remaining limits include CCD dense-output energy error, explicit bounded-solve
failure, constrained integration approximation, high-cost collision tails and
incomplete mesh-rig sleep and the cold-save continuation failure. Human M77/current-candidate review and Windows physical
hardware validation remain pending. No later milestone was begun.

## Review handoff

- [Current results and exact identities](evidence/m78-continuation/RESULTS.md).
- [Exact changed-file inventory](evidence/m78-continuation/CHANGED_FILES.txt).
- [Final fingerprints](evidence/m78-continuation/FINAL_FINGERPRINTS.json).
- Fresh Linux package: `/tmp/judas-m78-continuation-balance-midpoint/judas`.
- Owned editor project: `.cache/m78-continuation/editor-balance-project/balance_lab.judasproj`.

Focused human checks: F1, Stride selected, P, wait, B, wait for standing, J;
verify the unchanged perturbation causes stepping then stable standing. F2 checks
moving/rotated support, F3 P/G checks bounded recovery, F4 checks radial gravity.
Exercise pause, save/load, reload and Stop; repeat using the exported package.
Recorded failure cases such as insufficient grip/blocked recovery must remain
honest finite failures. Automated evidence is not human acceptance.

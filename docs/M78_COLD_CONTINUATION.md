# M78 — cold-boundary corrective continuation

Starting HEAD remains `9e77b2226edcf217ab789fe4f0fea42a3415a72d`.
This work is **uncommitted and unstaged**, with human review pending. It continues
M78; it does not start another milestone. The earlier [audit](M78.md),
[corrective report](M78_FOLLOWUP.md) and [midpoint/contact handoff](M78_CONTINUATION.md)
remain frozen records of their respective candidates.

## What was investigated

The unchanged M77 application captures an in-progress corrective step, then
resumes it in a new process for 720 steps at 60 Hz. The rig, 29 N s disturbance,
controller settings, timing, inputs and acceptance thresholds are unchanged.
The original Windows snapshot is preserved byte-for-byte:

`docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save`

SHA-256: `a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b`.
It fails **13 checks / 1 failure** on both Linux and native Windows, although the
same native live state reaches standing when continued uninterrupted. The
original Linux-generated failure is a different snapshot and is kept separately.

Seven body physical states, six joint settings and semantic controller history
roundtrip exactly. This eliminates an ordinary pose/velocity reconstruction bug.
The first solver boundary exposes the missing state:

- The live continuation has **36 rows**, including **14 angular-drive rows**.
  The old cold restoration has **22 passive rows** and no drive rows.
- The actual latched drive target is not always the newly recomputed request.
  Existing target hysteresis retains one target differing by **0.000595620 rad**.
- The ten-pass sequential-impulse result depends on its initial accumulated
  impulses. That dependency is measured, rather than inferred from the existence
  of a cache.
- Restoration clears the controller observation and skips its first preparation.
  A genuinely completed physics publication, even with no *external foot*
  contacts, is different from a new world with no completed observation.

The saved publication in the new Linux reproduction is not fabricated empty
support: it contains epoch **546**, the actual 1/60-second completed timestep,
and a real left-pad/right-pad contact with **0.1966645867 N s** normal impulse.
Balance excludes contacts within its own articulation, so its external support
loads are zero at that particular boundary.

## Experiments and causal limits

Diagnostic sources were copied into owned cache directories. Original source,
tests and snapshots were preserved while individual mechanisms were isolated.
All genuine negative results remain in the evidence directory.

| Intervention | Observed result |
| --- | --- |
| Restore actual commands only into the old snapshot | Still fails |
| Unconditionally skip the restored first-observation wait | Still fails; also breaks existing controller contract |
| Solve the first/every joint quadratic system to near-exact KKT convergence | Still fails the actual nonlinear controller continuation |
| Passive joint history only, old exact snapshot | Can rescue this one boundary; not sufficient generally |
| Passive history only, newly emitted save | Cold-write 24/0, cold-read 13/1 |
| Passive history + latched commands, original first-wait phase | Still fails, with or without drive history |
| Full joint context, normal first preparation, no drive history | Still fails |
| Full joint context + normal first preparation | New capture/read 24/0 and 13/0; current physical standing matches source |
| Same context + actual saved completed publication, guarded first preparation | Controller 119/0; new capture/read 24/0 and 13/0; old snapshot still 13/1 |

The independent row analysis uses
`A = J M^-1 J^T + diag(gamma)` and the actual impulse bounds. The source system's
condition number is about **2782.71** (diagonally scaled **184.63**).
Actual ten-pass warm-start objective gap is **0.729935 J**, versus **0.202516 J**
from zero. Thus reaching standing does **not** mean the warm solve is closer to
the frozen linear optimum. A near-exact frozen solve also fails later. This
repair preserves bounded algorithm continuation; it does not claim to cure the
constrained solver's numerical/model limitations.

A separate hinge probe exposes warm-start influence on predictive limit-row
construction. It does not introduce the first M77 divergence: that boundary
has no active limit rows. It is recorded, not folded into an unrelated solver
rewrite.

## Generic repair and compatibility

The retained implementation uses the existing save-participant architecture,
existing durable entity/joint mappings and generation-aware reconstruction.
There are no demo names, captured constants, test flags or replayed future
controller commands in the implementation.

1. A bounded, versioned joint-continuation record stores the **actual latched
   angular-drive command**, the 12 passive and three drive numerical impulses,
   solver-format identity and original regular timestep. Physical/settings
   context is validated before use. A different timestep discards numerical
   guesses; it does not scale impulses or discard valid actuator intent.
2. A separate record stores the **actual completed contact observation**:
   local anchors, real normal/load/separation, shape parts and completion
   epoch/timestep. Durable body IDs are remapped to current generations. This
   is read-only observation data; it is never installed in either contact
   solver warm-start cache and never causes an impulse or callback replay.
3. Reconstruction applies continuation after bodies, articulations, controllers
   and deformables have been reconstructed, before private-world publication.
   A restored-completed token permits the ordinary first controller preparation
   only at the matching timestep. It expires at the next physics step.
   Standalone controller-history restoration retains its existing wait.

Existing articulation and contact participants are versioned; no extra save
participant slots are introduced. Old saves remain readable through their
existing cold-start path. A new-format save requires its matching context;
missing, truncated, incompatible or newer data rejects the private restoration.

**Legacy gap:** the original `a106…` snapshot never recorded this context.
There is no unique way to reconstruct its finite-pass dual state from positions,
velocities and semantic history. It remains a genuine failing legacy witness.
Passing new captures must not be reported as repairing those absent bytes.
There is no universal bitwise-continuation promise: changing joint insertion
order, timestep, filtering or physical context changes the numerical problem.

## M70 anchor reconstruction follow-up

The combined regression checks exposed a separate M70 cold-read rejection:
**10/0 capture, 14/3 read** on Linux and native Windows. Only the first rejection
was causal; the two script-state failures followed because restoration stopped
before the script participant. The actual error was `joint continuation physical
context differs`.

A decoded comparison found unchanged joint settings, mass, inertia, filtering,
shape and static authority. Only the prescribed static anchor's physical pose and
velocities differed. The saved scene contained its current presented pose, but
`GetEntityState` returned a slotless entity's initial cached state. The existing
physical record then overwrote that correct scene pose, and static restoration
cleared its velocities. The new exact context guard exposed this pre-existing
capture/reconstruction omission rather than causing a physics instability.

The narrow correction uses actual PhysicsWorld pose/velocities for Full ordinary
rigid bodies in the existing physical record. Reduced-fidelity cached motion,
no-body entities and CharacterMotor root state retain their own authority. Motor
query capsules are representations offset from the root, not replacement root
state. Derived query geometry/classification is reconstructed from the restored
motor and authored settings before callbacks. No authority, solver or controller
settings change. Original failures and the discriminating static/coarse/motor
probes remain in the evidence tree; final combined results are recorded below.

Older files which already recorded stale anchor state cannot recover absent
velocities. New captures establish the corrected contract; no retroactive exact
restoration claim is made for those old records.

## Performance and platform scope

Linux host timing is the current performance gate. The preserved four-ragdoll
workload retains four original 13-body articulations, 48 constraints, the
8,192-triangle floor, contact fidelity, CCD requirements and 900 steps at 60 Hz.
Original p95 was **18.875046 ms**; the prior combined candidate measured
**5.498837 ms**. The actual combined save-repair build measured median
**1.085516 ms**, p95 **5.454081 ms**, maximum **28.235067 ms**, with 11/900
steps above 16.67 ms. The final static/motor follow-up archive measured median
**1.129538 ms**, p95 **6.184333 ms**, maximum **28.697686 ms**, with **12/900**
steps above 16.67 ms. Native Windows functional work was running in the guest
on the same host during this last measurement; the load average is retained.
Both runs pass the unchanged **16.67 ms Linux p95 gate**. No mesh, articulation,
contact fidelity, CCD requirement or cadence was reduced.

| Same Linux four-rig workload | Median ms | p95 ms | Maximum ms | Steps >16.67 ms |
| --- | ---: | ---: | ---: | ---: |
| Original failed corrective candidate | 1.040693 | 18.875046 | 33.793832 | 78/900 |
| Paired Boolean-only correction | 1.032763 | 14.963051 | 23.999817 | 30/900 |
| Actual combined save repair | 1.085516 | 5.454081 | 28.235067 | 11/900 |
| Final combined static/motor save follow-up | 1.129538 | 6.184333 | 28.697686 | 12/900 |

The final run retains **56,576 contact rows**, **19,163 CCD search iterations**,
seven bounded fallbacks and **26/52 sleeping bodies**, matching the previous
actual combined run's aggregate physical counters. In its slowest 5% of steps,
the solver scope accounts for **90.89%** of wall time. That scope includes
continuous-impact search/scheduling and event solves; it is not isolated linear
constraint cost. The worst step is 85: **830 CCD search iterations**, one fallback
and **27.456 ms** inside that scope. Collision tails and incomplete mesh-rig
sleeping remain explicit limits. Only the paired Boolean-only isolation
establishes that optimization's separate causal effect; the combined physical
trajectory changes too. No unrelated optimization is added in this follow-up.

The Windows 10 VM is a **native functional-validation environment**, not a
performance acceptance platform. Its original **56.376 ms** maximum (reported
rounded as 56.38 ms) is preserved. Two earlier repeats and raw counters are also
retained, but the VM is not being tuned to the 50 ms native-host comparison.
Wall timing alone cannot distinguish scheduling noise from solver CPU cost.
No physical GPU/controller/audio or human Windows acceptance is claimed.

## Deferred problem

**FLU-001 remains failed and unchanged.** The original Linux four-second speed
check and its separate proposed settling criterion remain
[recorded](evidence/m78-followup/FLU-001.md). No fluid source, fixture, tolerance,
protected research or accepted fluid evidence was changed by this continuation.
The prior native pass does not close the Linux failure.

## Final source-linked verification

The final affected Release compilation/relink is incremental, using preserved
unchanged objects and pinned dependencies. It is not a new clean full-engine
build. The earlier combined physics checks remain recorded against the exact
unchanged PhysicsWorld/PhysicalBalance code; the final WorldPersistence-only
correction receives its own affected reruns.

The affected Linux production compile is warning-free. Native Windows reports
one new harmless local-variable shadow warning in the persistence capture path
and two pre-existing conversion warnings in the unchanged legacy script-slot
path. The read-only endpoint observer has an unused-parameter warning. These
receipts are preserved; no global zero-warning or fresh full-clean-build claim
is made.

Linux final results include:

| Affected final check | Result |
| --- | --- |
| New continuation/authority/query guards | 98 checks / 0 failures |
| World persistence / reduced-fidelity streaming | 27/0 / 82/0 |
| Gravity selection | 70/0 |
| M77 ordinary corrective step / lifecycle / radial | 15/0 / 26/0 / 18/0 |
| M77 in-progress new capture / fresh-process continuation | 24/0 / 13/0 |
| Standing and recovery new cold pairs | 23/0 + 13/0; 21/0 + 13/0 |
| M70 application / new cold pair | 98/0; 10/0 + 14/0 |
| Original a106 legacy cold read | **13/1, still failed** |
| Editor Play/Stop, export, moved standalone and scene/save/reload smoke | Passed (offscreen software rendering) |

The earlier source-linked combined checks also pass: controller 119, foundation
34, rigid motion 10,239, physical animation 64, joints 29, ragdoll 36, contact
lifecycle 1,076, impact timing 89 cases/3,197 checks and prefab 37. Their original
receipts/binary identities remain separate from the final narrow reruns. The
passing angular, contact, prefab, streaming and navigation corrections are kept.
No full production-suite marathon was repeated.

A separately linked instrumentation-only observer verifies the actual final
Linux endpoint: uninterrupted and fresh-process body states are **exactly equal**,
controller status is **Succeeded**, actual upright is **0.998584**, actual support
height **1.361587 m**, accepted contacts **4/4**, and relative COM speed
**2.14e-6 m/s**. This verdict reads current bodies/completed contacts, avoiding
the unchanged application's cached-observation `Standing` helper. See
[the final current-state witness](evidence/m78-cold-boundary/final-current-standing/README.md).

Native Windows final functional validation uses MSVC Release code on the same
frozen source. Its [final matrix](evidence/m78-cold-boundary/WINDOWS_FINAL_V2_MATRIX.json)
records 28 focused executions: 27 pass and one preserved legacy failure, with
the earlier unchanged core checks distinguished from the fresh affected reruns.
The affected matrix passes world persistence, streaming, all
ordinary M77 modes, its three new cold pairs, the M70 application/new cold pair
and **98/0** continuation guards. The original a106 read still fails **13/1**.
Editor Play/Stop and ordinary export/moved-package startup pass. The package
runs from unrelated `C:\Windows\Temp` with an unavailable engine root,
exercising four scene loads, two save/load services, pause/resume and reload.

The separate native current-state observer also reports **Succeeded** and exact
uninterrupted/fresh-restored endpoint equality within its paired run: actual
upright **0.998602**, support height **1.361870 m**, both feet supported and
relative COM speed **7.14e-7 m/s**, with one landing. Native cold-write/read are
**24/0 and 13/0**. [Its full state comparison](evidence/m78-cold-boundary/final-current-standing/WINDOWS_ACTUAL_ENDPOINT_PARITY.json)
uses actual physics data, rather than a cached standing observation. Linux and
Windows endpoints are not claimed to be bitwise equal to one another.

Package preflight twice stopped because of insufficient guest free disk, before
export/runtime work; both tooling receipts are retained. Only named owned
obsolete generated objects/executables were retired after full host archival
and fresh hash/byte-count verification. This was environment housekeeping, not
an engine correction. Native artifact identities and exact commands are kept
under `native-final-narrow-wp/`. Physical GPU/audio/input and human gameplay
acceptance remain outstanding.

## Remaining gaps

- The exact original a106 save still cannot recover omitted continuation state;
  a universally cold-robust physical controller/solver is not established here.
- Earlier captures with stale prescribed-static records cannot recover absent
  motion retroactively. New captures are the corrected contract.
- The motor query fix is private cold reconstruction. The ordinary live explicit
  transform setter's existing query-offset refresh window is not broadly changed.
- No universal bitwise continuation guarantee is made for changed timestep,
  topology/insertion order, filtering or physical context.
- Mesh-ragdoll CCD tails and incomplete sleeping remain as measured above.
- FLU-001 remains unchanged and failed; Windows hardware and human review remain
  outstanding.

## Review artifacts

- [Investigation and reproduction index](evidence/m78-cold-boundary/README.md).
- [Independent review](evidence/m78-cold-boundary/independent-review.md).
- [Windows timing scope](evidence/m78-windows-tail/REPORT.md).
- [Final affected Linux results](evidence/m78-cold-boundary/final-after-static/LINUX_MATRIX.json).
- [Native Windows final report](evidence/m78-cold-boundary/WINDOWS_FINAL_V2_REPORT.md)
  and [source-linked matrix](evidence/m78-cold-boundary/WINDOWS_FINAL_V2_MATRIX.json).
- [Linux editor/export smoke](evidence/m78-cold-boundary/final-after-static-smokes/RESULTS.json).
- [Final Linux performance](evidence/m78-cold-boundary/final-after-static-performance/RESULTS.json).
- [Exact changed-file inventory](evidence/m78-cold-boundary/CHANGED_FILES.txt) and
  [final fingerprints](evidence/m78-cold-boundary/FINAL_FINGERPRINTS.json), with
  [verification](evidence/m78-cold-boundary/FINAL_VERIFICATION.json).

Human checks remain: corrective stepping and recovery, save during an active
step then reload, moving/rotated support, Play/Stop/reload, and the moved
standalone. Automated VM execution does not replace human gameplay review.

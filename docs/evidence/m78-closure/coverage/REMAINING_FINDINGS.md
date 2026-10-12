# Ranked remaining findings after scoped Linux/native closure

These are remaining defects or measured limits, not authorization to implement
them in this review. Ranking reflects the concrete current evidence and likely
consumer impact; it is not a whole-engine severity survey. Native functional
routes are attached separately; these two new lifecycle defects were exercised
only on Linux. Human/hardware acceptance remains separate. See the [case index](CLOSURE_CASES.json)
for source, fixture, binary and receipt identities.

| Rank | Finding / kind | Current bounded result | Next decision needed |
| --- | --- | --- | --- |
| 1 | **LIF-001 — streamed CharacterMotor query publication; confirmed pre-existing defect, S2 / P2** | Final Linux **46 checks / 8 failures**. Before the first update, authored layer 1 / mask 2 instead reads layer 0 / all-category mask; an initially disabled motor is enabled and a retained active motor is disabled. Ordinary zero-dt `UpdateCharacters(0)` corrects the representation. | A separately authorized narrow creation/publication correction with staged invisibility, enabled/filter/tag and fresh-generation regression coverage. No fix is included here. |
| 2 | **LIF-002 — legal simple dynamic-actor save exclusion; confirmed pre-existing persistence defect, P2** | Linux **22 checks / 1 failure**: retained control **12/0**, excluded actor **10/1**. Public exclusion and capture succeed, but the orphan sleep record makes private full restore reject `saved sleep body unavailable/duplicate`. Relevant ownership mechanisms match HEAD. | A separately authorized participant-eligibility correction with retained/excluded and articulated-exception controls. No fix is included; native/service/UI/live-world-loss scope is untested. |
| 3 | **PERF-001 — mesh-ragdoll CCD tails and incomplete sleep; measured limit** | Latest unchanged-workload serial final wall p95 **7.0623 ms** passes the unchanged **16.67 ms** gate, but maximum is **33.3996 ms**, **18/900** steps exceed the deadline and **26/52** bodies sleep. Earlier raw p95 **17.700774 ms** / maximum **95.307776 ms** miss remains. | Decide whether the residual tails matter in a representative consumer workload. Do not claim an engine performance regression or exact old-stall cause from host-load correlation alone. |
| 4 | **FLU-001 — legacy PBF settling criterion; deferred unresolved contract/model question** | Frozen Linux result **0.558701396 m/s** fails the unchanged **<0.5 m/s after 240 steps at 60 Hz** criterion; no fluid retuning occurred. Conditional still-liquid control also misses that terminal deadline, without proving the coupled model correct. | Separate bounded contract/model adjudication remains explicitly deferred. Do not change damping, threshold, duration or protected research to manufacture a pass. |
| 5 | **LEG-001 — original `a106…` M77 cold snapshot; historical compatibility limit** | Byte-preserved original snapshot remains **13/1**. New-format standing/step/recovery and mixed-motor cold pairs pass within their own Linux/native envelopes. Separate actual endpoints strengthen physical standing, not universal trajectory equality. Expected exit 1 is still a failed assertion. | Retain the absent-history limitation and versioned refusal/discard policy. New passes cannot reconstruct continuation bytes the original file never stored. |

## LIF-001 scope and counter-evidence

[Final sequenced readback](../audit-lifecycle/linux-current-01/run-04/streaming-fixture/RESULT.json)
and the [archived account](../audit-lifecycle/linux-current-01/SUMMARY.md) retain the
failed assertions. Rotated shape-offset pose, retained authoritative root/velocity,
fresh identity and old-handle rejection **pass**. No simulation time advances in
the publication/revisit observer. The combined layer-plus-tag cast is a real
query oracle; tags are not independently measured because the wrong layer/enabled
state already blocks the cast. The tag initialization diagnosis is source-derived.

`WorldStreaming.cpp`, `WorldRegions.cpp`, `WorldCharacter.cpp` and `WorldScripts.cpp`
are byte-identical to HEAD in the [source identity receipt](../audit-lifecycle/linux-current-01/AUDIT_RECEIPT.json).
This establishes a pre-existing query metadata/enabled publication gap on the
selected lazy/region route, distinct from the repaired modern disk-save route.
It does not demonstrate a crash, fluid accounting error, physical response error
or unbounded growth. Original scratch diagnostic values from run-02 are preserved
as an observer-ordering mistake; run-04 records actual fields explicitly.

## LIF-002 scope and counter-evidence

[Measured result](../audit-lifecycle/save-exclusion/MEASURED_RESULT.md),
[all assertions](../audit-lifecycle/save-exclusion/linux-current-01/fixture/RESULT.json)
and the [source comparison](../audit-lifecycle/save-exclusion/linux-current-01/SOURCE_MECHANISM_COMPARE.json)
retain the actual rejected legal case and positive control. A normal simple actor
is created through public ownership APIs, excluded through the native primitive
behind JudasJS `saves.exclude`, then captured and restored in a separate private
world. There are no controllers, joints, scripts, fluids, regions or simulation
steps in this discriminating fixture.

Scene capture omits the non-articulated transient actor, while sleep capture
retains its owned dynamic body; restoration cannot find a retained body mapping.
The original capture-only exclusion test did not exercise this read. Current
WorldPersistence also contains accepted M78 repairs, but these three relevant
selection/guard mechanisms are byte-identical to HEAD, establishing a pre-existing
participant-ownership mismatch rather than a demonstrated M78 regression.

This can produce a successfully captured but unreadable slot while the actor
exists. It does **not** demonstrate live-world loss or SaveService/UI callback
semantics: the observer restores a separate private RuntimeWorld. No native run,
correction, loosened missing-body guard or sleeping-policy change was made.

## PERF-001 scope and attribution

[Raw final timing](../final-performance/RESULTS.json),
[host observations](../final-performance/HOST_CONTENTION.json) and the
[serial CPU/wall follow-up](../cpu-observer/RESULTS.json) are all retained.
The workload is the same four rigs, 52 bodies, 48 constraints, 8,192 mesh triangles,
900 steps at 60 Hz. The serial prior/final wall p95 values are **6.31734 / 7.0623 ms**;
final thread-CPU p95 is **6.91117 ms**. Both variants retain 56,576 contact rows,
19,163 CCD iterations and seven bounded fallbacks. The prior separate **6.184333 ms**
pass remains historical evidence.

Eight external compiler processes and roughly 46% CPU pressure were observed
after the raw miss. Earlier per-step CPU clocks were not recorded, so the later
serial pass cannot assign the exact old stalls to those processes. CPU clocks are
observational, not a replacement for the wall-time gate. No engine performance
implementation change, cadence reduction or scheduler/resource change was made
for this diagnosis; native VM timing is not a Windows hardware performance gate.

## Deferred and compatibility boundaries

[FLU-001](../../m78-followup/FLU-001.md) remains legacy approximate PBF; it is not
the M54 conserved ledger or M55 dynamic surface. Protected FTFT/P1 evidence and
the previous accepted fluid work remain separate and unchanged.

[The original snapshot failure](../final-applications/LINUX_MATRIX.json) and
[final smoke classification](../final-smokes/RESULTS.json) remain visible.
The fresh mixed-world physical endpoint is exact within its paired fixture for
reported body fields/contacts, excluding fresh native handles; this does not
promise universal cross-platform, topology, order or timestep bitwise continuation.

Other [coverage gaps](REMAINING_COVERAGE.md)—unexecuted public-member semantics,
authored fields/gestures, asset paths, missing original briefs and pending human
review—are knowledge/acceptance gaps and are not relabelled as measured defects.

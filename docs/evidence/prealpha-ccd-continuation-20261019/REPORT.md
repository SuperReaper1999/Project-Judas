# Curved-mesh CCD continuation follow-up

## Scope and status

Development parent: `146a0727eab63d9555c732bf73e6093be5bc489d` on Experimental.
Main remains `4c141e9bb6c68a50c63dec263dcec398200328ad`. This is automated Linux
qualification, not human acceptance, general CCD certification or Alpha readiness.

## Established mechanism

The unchanged original suite fails four assertions at the parent (372 checks /
4 failures). Its coarse/medium/dense curved sheets let 11/20/15 of the original
24 thin boxes cross during their original 90 steps. The independent actual-sheet
oracle, rear-entry and finite-edge negative controls are retained. The reduced
16-triangle reproduction also crosses; triangle count alone is not the cause.

A local support contact could retire an entire concave mesh from later collision
search. Repeating those searches alone still fails all four. The previous single
maximum penetration allowance also lets one deeply overlapping triangle hide a
new or shallower finite triangle's growing penetration. The paired two-face tests
explicitly distinguish that failure from merely adding more queries.

## Adopted correction

- Keep static mesh versus box/convex-hull pairs eligible after local contact.
- Capture each actually accepted native triangle's maximum witness depth plus
  the existing feature-sized growth allowance. Newly encountered triangles receive
  only that allowance. Do not inherit an unrelated face's depth or an infinite
  plane's overlap. Observe all accepted witnesses before response reduction.
- These limits belong to one trajectory query. Rebuild after physical trajectory
  changes; add no cross-step contact ancestry, save schema, ownership epoch or JS API.
- Deduplicate identical initial event queries; retain unconditional rescheduling
  after real event-induced changes.
- Skip searches only with a sufficient bound on actual native response depth,
  including the existing rear-normal guard and clipped witnesses. The whole-time
  bound initially supports centred boxes; unsupported or uncertain cases retain
  the existing search. At-point depth bounds also avoid unnecessary SAT work.
- Fix a separately demonstrated clipping defect: epsilon classification could
  generate an intersection parameter outside [0,1], inventing points outside the
  incident polygon. Retain the endpoint in that case. Genuine segment crossings
  keep the original arithmetic. The small shared internal ClipPolygon header
  lets ordinary tests exercise the actual kernel without including an entire cpp.

No timestep, controller, solver/event/sampling budget, collision margin, object
count or physical acceptance threshold was reduced. The existing bounded sampling
fallback remains an approximation and is not represented as complete continuous CCD.

## What was deliberately not adopted

The earlier history-aware prototype passed the original four but introduced an
unrelated-filter lifetime failure and significant cost. Subsequent variants fixed
that lifetime test, but the strict new-face rule also removed the original
history-sensitive checkpoint. Its old save test's influence precondition then
failed: this was not evidence that new persistence was broken. An explicit
history-disabled ablation still passes the original four, so the complicated
ancestry/persistence/failure-quarantine machinery is excluded from production.
The old successful and unsuccessful experiments remain available in the archives.
Do not use older 95-check or 14-check history receipts to certify this candidate.

## Correctness evidence

- Original suite: 372/4 before, 372/0 in the final isolated implementation;
  original first-arrival/curved subsets also pass.
- The same repeated-search mechanism without feature budgets: 372/4.
- Native feature controls: 128/0, including the two-face causal controls, boxes,
  hulls, arbitrary frames, real existing depth, absent finite faces and rear entry.
- Actual clipping kernel: 15/6 before, 15/0 after. The new header tests exercise
  that same corrected algorithm.
- Interval bound: 65/0, with 4,097 represented-time observations per case,
  positive safe support and negative spin/new-face/two-sided/normal-guard cases.
  Dense observations are falsification controls, not the mathematical proof.
- At-point optimization: all 8,192 paired predicate/response/budget rows match
  the unoptimized reference exactly; fingerprint 0166274568e9538b. The source
  additionally executes the unchanged 128 feature controls (132 total assertions).
- The optimized corrected implementation preserves all 120 measured physical
  step digests of the unoptimized corrected coarse/medium ragdoll trajectories.
- Twelve affected isolated regression suites pass, including 4,412 impact
  robustness checks, actual native continuation, ragdolls, contacts and CharacterMotor.

Final ordinary separate-translation-unit Release runtime/editor/exporter builds succeed with zero compiler warnings. All nine provenance-checked gates pass, including the original 372/0 suite. Ten additional native regressions and the actual fresh-process streamed-game write/read pass. The six final whole-suite trajectory digests equal the isolated candidate. Details are in FINAL_RESULTS.json.
The original world fixture and original failed logs have not been rewritten.

## Performance: correctness costs are not hidden

Same i7-4790, GNU 15.2 Release, original 13-body / 12-joint fixture, original 60 Hz
inputs and 60 steps, three alternating pairs, 180 timed steps per side/scenario.
Raw times, counters, RSS, commands, source and binary hashes are preserved.

| Scenario | Parent median / p95 / worst ms | Corrected median / p95 / worst ms |
|---|---|---|
| Coarse, 104 triangles | 3.492 / 8.804 / 14.139 | 4.448 / 17.262 / 29.291 |
| Medium, 1,800 triangles | 3.871 / 16.506 / 21.028 | 6.387 / 19.997 / 35.218 |

The initial smaller correction was substantially slower still (coarse 6.537 /
19.425 / 37.905; medium 8.640 / 29.794 / 39.796 ms in its matched run). Bounds
and initial deduplication reduce its redundant work without changing corrected
trajectories. This does NOT demonstrate a speedup over the incorrect parent.
The corrected solver handles more actual impacts, and parent trajectories differ.
Penetration in this workload improves, but a 60 Hz articulated-mesh guarantee is
not established. Severe 1,024-piece pile tails remain a separate open finding;
no new dense-pile speedup or universal worst-case bound is claimed.

## Independent retained-origin precision repair

Real regional suspension and cold reconstruction at origins around 1e12 revealed
that `(local + captureOrigin) - currentOrigin` loses small local coordinates even
for zero origin delta. Three reconstructions now use
`local - (currentOrigin - captureOrigin)` before narrowing to float.

The identical portable test fails 7 of 67 checks before and passes 67/0 after.
It checks actual reconstructed native bodies, entity/native agreement, fresh
runtime identities, exact position bits, velocities and a second cold restore.
Zero and supported 512-unit origin changes are exercised. The earlier fractional
origin test was invalid because requests snap to 512-unit anchors; its failed
results are preserved and the replacement asserts the actually committed origin.
The runtime origin policy and save/archive formats are unchanged.

Origin/stream (23), streaming (82), terrain/stream (35), and separate-process
streamed-game write/read (11/19) checks pass on the one-file isolated repair.
The first application invocation lacked required arguments; a subsequent run
without owned user-data isolation hit a save-storage lock. Both setup failures
are preserved. Proper isolated runs use the existing XDG_DATA_HOME convention.

## Remaining limits and disposition

The four reported contracts are corrected within the declared native mesh/poly
scope. This does not resolve universal finite-feature continuation: native
one-sided rear selection remains, sufficiently deep initial overlaps are
ambiguous, two-sided responses share an authored-triangle allowance, and temporal
sampling can miss events shorter than its interval. Moving meshes and other
primitive combinations retain their existing mechanisms. These are not hidden by
larger budgets, a reset, a global plane or a claim of exact contact history.

Windows and Android were not requalified in this follow-up. Linux is the measured
platform. No new gameplay features, fluid changes, independent research edits,
protected-history edits, main promotion, tag or human-validation claim is included.
FLU-001, legacy a106 missing-continuation data and liquid query-identity limits
remain unchanged. PhysicsWorld.cpp remains the unchanged 859-line coordinator.

## Human review

1. Run the existing Physics Stress Lab (default room, F5) and confirm ordinary
   machinery remains usable. This room is not a visual reproduction of the curved
   numerical fixture; key 3 retains its known extreme dense-pile stalls.
2. Revisit a mesh-heavy existing game (Skate/parkour), including curved supports.
3. Try ordinary CharacterMotor and a physical assembly; verify contacts and motion.
4. Save, restart and revisit a streamed region; verify actual object placement.
5. Judge the documented articulated-mesh cost before any main promotion.

See REPRODUCE.md, FINAL_RESULTS.json, FINAL_FINGERPRINTS.json and the raw research
archives. Protected earlier reports remain historical records, not rewritten
success claims. The earlier prototype report is preserved beside this directory. Repeated unchanged source headers are mapped to the pinned parent in the research manifest; every changed source and raw receipt remains byte-preserved.

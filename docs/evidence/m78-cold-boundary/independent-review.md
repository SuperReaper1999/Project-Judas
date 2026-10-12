# Independent joint-boundary review

This review is of current source and passive diagnostic artifacts. It adds no
runtime change, build, test execution, cache persistence, gain change or iteration
budget change by this reviewer. The owning agent's later source application is
recorded explicitly below; original failed snapshots and historical evidence
remain unchanged.

This is a chronological review. Early sections describe the pre-repair candidate
and judgments made before later controlled experiments. The current mechanism
and source verdict is in **Final reviewed correction verdict** below; old
numerical conclusions and failure receipts are retained as historical evidence.

## Preparation and publication order (pre-repair candidate)

- `WorldPersistence.cpp:121` recreates articulated `JointSettings`; it does not
  recreate the enabled angular-drive command. `:142–143` configures the restored
  balance controller and restores its semantic history.
- `PhysicalBalance.cpp:367` clears the observation and sets its first-sample
  waiting flag. `:284` returns on the first resumed fixed tick before advancing
  reference/task clocks or setting angular-drive commands.
- `Simulation.cpp:32–38` runs fixed scripts, motor updates, animation references,
  physical-animation preparation and balance preparation before rigid physics.
- `PhysicsWorld.cpp:2193–2195` prepares contacts and joints, then performs the
  established ten interleaved sequential-impulse passes.
- `PersistTouches` preserves pair event summaries; it does not reconstruct the
  separate completed per-contact observation ledger. `ReadBodyContactSample`
  has no publication-epoch result: a valid body with an empty ledger returns
  `true` with an empty array. Empty samples alone therefore do not identify an
  unpublished fixed step.

The preserved source continuation's first observation has zero accepted support
contacts and zero support loads. Absence of restored contact loads alone is not
a demonstrated first-tick feedback difference for this exact boundary.

## Actual first solver systems (pre-repair journals)

The Linux passive journals show **36 rows / 14 angular-drive rows** in the source
continuation, versus **22 rows / no angular-drive rows** in the fresh restoration
of the exact native failed save. The latter consists of 18 anchor and four hinge
alignment rows. This is an authoritative actuator omission before comparing
warm-start guesses, not just an omitted numerical cache.

These two Linux journals start from different platform-generated saved physical
states. Their detailed velocity vectors and objectives must not be presented as
a matched source-versus-restoration physical comparison. The previous native
roundtrip and live warm-seed ON/OFF experiments remain separate evidence.

## Warm-start row topology

`JointSolver.cpp:20–22` and `:73–75` immediately apply warm impulses while later
rows are still being constructed. Predictive hinge/slider limits at `:47,55–56`
can therefore read seed-modified velocities. Contact warm impulses have already
been applied by `ContactSolver::Prepare` at this point. This is a real generic
preparation-order inconsistency, independently demonstrated by the root's small
hinge fixture; an assemble-then-apply-joint-seeds change alone does not exclude
contact-seed influence.

It is **not the first-boundary cause established for this M77 snapshot**. Its
source and live warm-seed OFF journals have the minimum 36-row system, with no
active limit rows. The knees' pre-warm predicted angles remain inside the
authored ±2.7 rad limits. The new journal confirms that seeding changes one
prediction from about −0.4403 to −0.3478 rad, but does not activate a limit.

## Independent quadratic-program review

For a frozen, contact-free joint solve, the current row equations are consistent
with

```
A = J M^-1 J^T + diag(gamma)
c = J v_free + bias
minimize 0.5 lambda^T A lambda + c^T lambda
subject to the actual row lower/upper impulse bounds.
```

The analyzer uses the correct GLM matrix-column transpose, current inertia,
Jacobian signs, drive softness and accumulated-impulse update. Its bound KKT
signs are correct: gradient ≥0 at a lower bound, ≤0 at an upper bound. The
independent solution is strictly inside the drive bounds in the reviewed first
source system. Both matrices are positive definite. Float inertia symmetry and
diagonal differences are at represented-value rounding scale.

The source system has condition number about **2782.66**, or **184.60** after
diagonal scaling. The restored passive system has about **842.61 / 10.45**.
Raw matrix conditioning depends on the mixed linear/angular row units; neither
number is a universal physical error certificate.

The double replay reproduces the source's actual ten-pass impulses within
**3.48e-7** and velocity closure within **3.64e-6**. On that same frozen source
system, the actual warm-start result's objective gap is **0.729139 J**, while
ten passes from zero give **0.203054 J**. The restored passive system's gap is
about **7.04e-8 J**. This supports the prior residual observation: the warm
trajectory reaching standing does not mean its first solve is closer to the
linearized constraint optimum. Conversely, the frozen QP optimum alone does
not establish a successful nonlinear controller continuation.

The analyzer initially labelled the first body slot as the root and emitted
non-standard JSON `Infinity` for an unbounded margin. Those new diagnostic
artifact issues were corrected: the root is explicitly identified as source
body 5 / restored body 1, and an unbounded margin is `null`. Numerical matrix,
objective and closure results were unaffected.

## Correction assessment (initial historical stage)

At this initial stage, Jacobian/effective-mass/softness/bound/impulse application arithmetic is
consistent with the standard accumulated-impulse method. The retained evidence
establishes actuator/first-publication omissions and finite-pass warm-start
sensitivity. Prior command-only and waiting-bypass overlays did not repair the
unchanged continuation check. No reviewed observation justifies serializing
disposable warm caches, increasing production iterations, inserting a delay,
retuning the controller or weakening its acceptance threshold.

At that stage, no sufficient minimal production correction had been established.
The cold-save failure must remain explicit unless a subsequent controlled
experiment demonstrates both the mechanism and a passing generic repair.

Reviewed artifacts: `linux-source-joint-boundary.jsonl`,
`linux-native-restore-joint-boundary.jsonl`, their `*-first-qp.json` analyses,
`probes/prepare_boundary_journal.py`, `probes/analyze_row_qp.py`,
`root-row-topology/`, and the frozen previous continuation diagnosis/receipts.

## Exact native paired boundary (pre-repair journals)

The later native source/seed-OFF/restored receipts remove the cross-platform
input caveat for that comparison. After handle remapping, their authoritative
pre-Prepare body fields are identical. Source and seed-OFF have identical 36
row equations and authoritative commands; only their 36 initial impulses differ.
Fresh restoration has the same 22 passive row equations but omits the 14 drive
rows. The native source matrix condition is **2782.71 / 184.63** after scaling.
Its warm ten-pass gap is **0.729935 J**, versus **0.202516 J** for actual seed-OFF.
Velocity/impulse closure is **3.82e-6 / 2.07e-6** respectively. This independently
confirms that lost warm guesses and the missing first command are distinct
mechanisms, and that better first linearized convergence alone is not a repair.

The owned first-boundary direct-SPD oracle reaches an objective gap around
**1.4e-12 J**, yet both source and seed-OFF ultimately have failed controller
status. The owned all-boundary box-QP diagnostic also fails genuine physical
standing despite its reported bounded subproblem KKT residual ≤**1.57e-13**.
Those are rejected diagnostics, not replacement production solvers.

## Closed-loop and saved step review

`PersistHistory` preserves the step/transfer/reference clocks, root/support
reference orientations, start/landing/stance/root/COM reference positions,
lift and landing history, initial targets and post-step transition targets.
The diagnostic decoder used for the target analysis reproduces the exact
native saved history SHA. No additional missing semantic step field was found.

The exact native snapshot restored on Linux completes its first physical
landing at trace frame 25, plans a second step around frame 70, then fails at
frame 201 with `swingTime=2.11666536`, just beyond the unchanged 2.1 s touchdown
budget. The live next-step seed-OFF trial instead never lands and fails around
frame 109 at that same budget. The common error string combines support, reach
and touchdown failures; it does not itself prove which branch failed.
`probes/prepare_balance_failure_observer.py` adds branch-specific failure
observations to an owned source copy, retaining all predicates and arithmetic.
Its receipts must be checked before claiming a precise later false-return cause.

The later owned failure-only observer reproduced the unchanged exact native
input failure and tagged **`touchdown_deadline`**. At its first failure:
`swingTime=2.11666536`, one prior landing, lifted=true, accepted contact=true,
planned support matches=true, and the actual contact point remains
**0.696397 m** from the goal versus the unchanged **0.2 m** tolerance. Its
support-handle/reach/bend branches have all passed. The restored ground's native
body slot is zero and is valid; zero slot alone must not be read as a null
generation-aware handle. The instrumented ordinary step remains **15/0**.
These are that copied observer's measured values, not claims of bit-identical
late trajectories between independently compiled observer builds.

The first-publication phase diagnostic advances continuous actuation/clocks
while withholding contact-dependent transitions. It still fails exact native
continuation and violates the existing focused test's explicit no-clock-advance
first-restored-sample expectation. It is rejected and not retained.

## Target-level correction and IK geometry

Actual joint targets are relative quaternions. Left multiplication of every
desired orientation by the same correction cancels:

```
inverse(C * desiredChild) * (C * desiredParent)
    = inverse(desiredChild) * desiredParent.
```

At the exact native first boundary, both support samples are empty, so all six
target quaternions are unchanged when that correction is removed (maximum
double algebraic difference **1.12e-16 rad**). The contacting-foot branch is an
essential exception: at `plantedOrientationWeight=0` it substitutes the actual
foot orientation. In a separately identified captured Linux stance tick 2,
removing the common correction changes the grounded ankle target by
**0.1576576 rad**; all other targets still cancel. Thus the strongest initial
claim of feedback disappearing from every drive was incorrect. Grounded ankle
actuation provides a plausible physical pivot policy.

`SwingReferences` overwrites the earlier fractional pose policy, then the step
path applies full common correction. `trunkLeanFraction`/`legLeanFraction` are
therefore not applied as authored during this path. Both are **1** in the exact
failed fixture, so restoring those fractions cannot affect its continuation.

IK first solves fixed world foot references; applying common orientation
correction afterward implies a rotated reference endpoint
`R + C * (F - R)` around its reference root `R`. Decoding the exact saved
root/COM/stance references gives implied first native foot-reference differences
of **0.317811 / 0.279047 m**, with the unchanged **0.2 m** touchdown criterion.
These are reference-geometry differences, not observed physical displacements,
not teleports, and not proof of the first-boundary failure: with no support
contact the common correction has no effect on any relative actuator target.

The root's coherent pre-IK root-attitude diagnostic keeps world foot goals
fixed, retains gains/thresholds and does not move physical bodies. It nevertheless
fails the exact continuation, ordinary application step and three controller
checks. It is rejected. The demonstrated reference seam is not a sufficient
causal production repair; altering control policy would require separate
justification beyond this batch's unchanged regression contract.

The follow-up reference-pivot diagnostic rotates the reference root position
and attitude using the same correction before solving unchanged world foot
goals. It also fails **119/3** controller, **15/1** ordinary step and **13/1**
exact cold checks; one landing is followed by an actual final root height near
**0.149 m**. It is likewise rejected. These coherent reference hypotheses lose
part of the existing corrective envelope and do not justify a retained patch.

The existing application `Standing()` helper at `BalanceLabTests.cpp:44` reads
the last controller observation without checking failed status or current body
upright/height. Since `Fail()` stops future `Observe()` updates, some rejected
diagnostics report a nominal passing count while their actor is failed and its
actual root has fallen. This review keeps the unchanged tests and thresholds,
but does not treat such a stale-observation pass as physical recovery evidence.

Additional artifacts: `native-first-boundary-comparison.json`, the three
`native-*-first-qp.json` analyses, `balance-target-feedback-analysis.json`,
`probes/analyze_balance_target_feedback.py`, and the root/step-agent owned
phased-publication, coherent-IK and numerical-oracle diagnostic receipts.

## Guarded first-boundary computational closure

The later owned replay explicitly checks the exact captured native first-boundary
body pose, velocities, mass/inertia and joint anchor/frame geometry before
reapplying its recorded angular commands and numerical seeds. It does not
replace body transforms/velocities and is not a persistence implementation.
Both Linux diagnostic variants pass **14/0** and reach current physical standing
with controller status Succeeded, so their passes are not stale-observation
artifacts. Captured commands+seeds alone gives two landings; adding the copied
normal first-Prepare phase gives one landing and a final root near
**(3.07956, 1.36084, 0.341165) m**, close to the exact native source continuation.

This establishes a causal computational-boundary closure for the exact case.
It does not establish a safe general cold reconstruction or justify blindly
serializing caches. The replay overwrites angular commands after the optional
normal first-Prepare phase too; its pass alone therefore does not prove that
the recalculated pre-guard restored targets match source. Commands+seeds alone
already passes, so clock/phase restoration improves trajectory similarity but
is not necessary to pass this single diagnostic acceptance result.

The production cold continuation remains failed and unchanged unless an
independently justified generic reconstruction/solver repair is demonstrated.

The old snapshot does not uniquely encode the previous finite-pass dual state.
From `v_after = v_free + M^-1 J^T lambda`, a change in the unknown previous
`lambda` can be offset by a change in unknown previous free velocity; current
pose/velocity alone therefore does not identify the original seeds or commands.
This is a limitation of exact numerical continuation from that archive, not a
proof that no cold-robust controller or solver could reach standing.

The causal closure could justify separately designed future persistence of
bounded solver continuation context, with explicit solver/timestep identity,
durable handle remapping, topology/filter/coordinate invalidation and genuine
publication/actuator state. It does not recover state omitted by this unchanged
old archive. Blanket serialization, hardcoded snapshot seeds or a new save
contract that leaves this requested old regression failing are not an
established production repair in this review.

## Later command/seed separation and new-capture repair assessment

The later split replay is narrower than the initial combined closure. In
`linux-first-closure-split-isolation.json`, commands-only fails with either
original or copied normal first-Prepare, while seeds-only passes with either
phase. The original-phase seeds-only final receipt has progress Succeeded,
current root height about **1.41814 m** and negligible current velocity. This
does not depend on the application's stale-observation Standing helper.

At that seeds-only replay's first boundary all six angular drives are disabled.
The drive impulse vector is therefore unused by JointSolver; enabling a drive
on the next controller preparation clears its impulse vector in
`PhysicsWorld.cpp:2706`. In this bounded case the effective passing intervention
is the **12 passive joint-history scalars**, with the original first-restored
contact/clock policy preserved. No physical body field is overwritten. This
supersedes the earlier inability to isolate which part of combined closure was
causal; it does not recover history missing from the original archive.

The small recalculated target mismatch is also a real command mechanism rather
than an unexplained rounding claim. `PhysicsWorld.cpp:2701–2706` normalizes the
request, keeps the previous target when `abs(dot(previous.target, target)) >=
1 - 1e-7f` and other drive fields match, and only installs a new request when
that test changes. The measured joint-5 difference of **0.000595620 rad** lies
inside this no-change deadband. An already enabled source can retain an older
target while a newly enabled restored drive installs the fresh target. The
seeds-only/normal-Prepare diagnostic passes with its newly calculated target;
exact source command replay is not necessary in that bounded test.

A **new-capture** continuation participant containing only passive per-joint
history is consequently a defensible correction to test, rather than blind
serialization of all caches. Existing `PersistJointSolverState` already saves
the same passive history for rigid fracture, though its reaction outputs and
fracture-specific diagnostics are not a reason to copy unrelated state into
articulation. A new participant must validate recreated JointSettings and
mapped bodies, finite state, solver identity and regular-step timestep, and
preserve existing disable/settings/teleport invalidations. It should restore
before the first ordinary solve and change neither physical bodies nor contact
publication, drive commands or controller clocks.

Native repeated split receipts and an unchanged cold-write/cold-read test of
newly emitted saves are still required before declaring that production repair
successful. The old a106 archive cannot acquire the omitted scalars; its failed
continuation must remain an explicit backward-compatibility gap, even if new
captures pass. No source implementation or test was changed by this reviewer.

The subsequent owned **new Linux capture** counterexample rejects a general
passive-history-only sufficiency claim. `new-capture-passive-cold-write.log`
passes its unchanged write workload, but `new-capture-passive-cold-read.log`
reports **13 checks / 1 failure**, status Failed and current root height
**0.248508 m** after one landing. The copied WorldPersistence adapter restores
only the 12 passive floats, no reaction fields, actuator commands or contact
ledger. The a106 first-boundary rescue therefore cannot be generalized to all
newly emitted archives.

Actual latched angular-drive commands are the next separately justified state
to isolate: enabled state, target, requested relative angular velocity,
stiffness, damping and torque bound all remain effective between preparations,
and the target deadband retains an earlier command. They are semantic actuator
state, not the same thing as a numerical warm-start vector. Any diagnostic or
production reconstruction must occur after balance Configure/PersistHistory
(Configure rejects a drive already enabled by another owner), but before
publication and the first ordinary solve. Existing validity rules should be
used. Drive impulse guesses remain a separate intervention requiring its own
causal evidence, not automatic inclusion with those commands.

The later copied new-capture command observer isolates both remaining inputs.
Passive history plus actual latched commands fails with the original first-wait
policy, with or without drive impulse guesses. With copied normal first-Prepare,
omitting drive impulses still fails (Failed, current root y **0.149143 m**), but
including them succeeds (Succeeded, current root y **1.360343 m**, physically
supported with negligible current speed). These receipts support a bounded
continuation context containing command and numerical state **and** correct
first-publication phase. They do not justify replacing the old unavailable
observation guard with an unconditional bypass.

A genuinely saved **completed physics observation** is a legitimate next
publication intervention. `completedContactObservations` is separate from
warm-start storage (`PhysicsWorld.cpp:944–950`) and feeds the next native
controller sample. Existing saved M42 touch pairs do not preserve its full
per-contact load, anchor, separation or completion epoch. A completed-empty
ledger must be distinguishable from a new world with no published observation.
The source's first derived foot supports are zero in this fixture, but that
alone is not proof that the entire completed ledger was empty.

Reconstruction should retain actual saved local anchors (double precision),
world normal, normal impulse, separation and shape-part identities, remap
durable entity IDs to new generations, and respect current filtering and
observation invalidation context. A/B order changes require consistent normal,
part and anchor conversion. Multiple manifold points may share a body/part key.
The ordinary reader must still derive point/support velocity using restored
physical state; no old controller Observation is substituted. Neither contact
solver cache receives this readonly payload. An invalid/unsupported publication
must remain unavailable, with the existing first-wait behaviour. No actual
completed-publication participant was retained or verified at this review point.

## Actual completed-publication prototype review

The subsequent copied prototype emits the actual completed observation ledger
and epoch/dt, rather than unconditionally bypassing the first-wait guard. Its
unchanged controller check passes **119/0**; new-capture write/read pass
**24/0 and 13/0**. The old a106 read remains **13/1**. This is a meaningful
new-capture causal proof, not recovery of the missing original archive fields.

`new-capture-publication-saved-observations.json` decodes a real completion
epoch **546**, dt **0.0166666675 s** and **one left-pad/right-pad self-contact**
with normal impulse **0.196664587**. It is not an empty global ledger. Ordinary
balance excludes bodies in its own articulation, giving zero external supports
without erasing that actual row. Double local anchors, remapped generations,
canonical A/B conversion, finite SaveArchive fields and bounded record count
are present. The prototype only restores readonly completed observations, not
contactCache/pendingCache. Old versions do not acquire a false completion epoch,
and standalone history restoration retains its original unavailable-first
behaviour.

Before retention, production needs these guards beyond the causal adapter:

- Capture must filter with the same enabled/nonsensor/queryOnly/CanCollide rules
  as the reader, rather than retaining a pair whose suppression changed after
  completion and then rejecting a legitimate restore.
- Apply continuation after all physical participants create required bodies,
  including rigid fracture, and before publication. The current copied order
  precedes deformable/fracture restoration.
- Validate captured shape/part, pose, mass/inertia, filter/material and body
  identity context before assigning current observation revisions. Old raw
  revision values are diagnostic provenance, not portable validity tokens.
- Include solver and regular-step identity for numerical guesses, and enforce
  required companion version/layout for new articulation captures.
- Remove diagnostic thread-local dispatch/environment flags from production.
- Compare actual first returned BodyContactSample values to source. Equal
  stored ledger bytes alone cannot prove broadphase-selected sample parity.

The source review establishes these concrete conditions and has not modified
production. Later retained-source validation, native repeats and malformed/
invalidation cases remain separate required evidence.

## Proper continuation proposal guard review

The reviewed proper source proposal is an owned copy in
`.cache/m78-cold-boundary/proposed-continuation`, rather than a production edit
by this reviewer. It contains no diagnostic environment dispatch. Existing
articulation version 4 embeds bounded, deferred joint continuation; contacts
version 2 carries the actual completed observation publication. Versions 1–3
retain their prior restoration semantics.

The joint payload separates the actual latched actuator command from the 12
passive and three drive impulse guesses. Existing owner/index/body identities
map the payload to recreated constraints. Bytewise context checks cover current
JointSettings, body pose/velocity, mass/inertia, collision shape and cooked
fingerprints, filtering and material. Restored guesses are invalidated for a
changed regular timestep and supported pose/settings/filter/mass/material
changes. The completed observations are separately remapped to current body
generations and revisions; neither contact warm-start cache receives them.

Continuation is deferred until balance Configure and physical/deformable body
reconstruction, then applied before saved sleep. This avoids an enabled-drive
ownership conflict and retains the actual command's no-change deadband rather
than recomputing its target. The normal first Prepare may advance only when the
world has a genuine, compatible restored completed publication; standalone
controller-history restore retains its original wait.

The proper owned proposal, including its exact body-context checks, passes the
unchanged Linux new-capture write/read **24/0 and 13/0** and standalone
controller **119/0**. These are receipts produced by the other agents, read by
this reviewer; no build or test execution was performed by this reviewer. The
old a106 archive still reports **13/1**. Exact-context compatibility has therefore
been demonstrated for the actual new fixture, but should not be generalized to
arbitrary unsupported reconstruction.

Two concrete retention guards were identified:

- The setter validates a normalized angular-drive target, while assignment of
  the saved actual command then preserves its original bits. A stored target
  must first be near unit, otherwise a malformed non-unit quaternion could
  bypass setter normalization. The reviewed owned proposal now requires
  `abs(dot(target,target)-1) < 1e-4` without changing genuine captured targets.
- If an include predicate omits an otherwise valid positive completed row,
  the remaining subset cannot be called a complete publication. Capture must
  encode unavailable publication (`false`, epoch/dt zero, no rows) in this case;
  joint continuation keeps its independently valid completed timestep. Invalid
  stale/suppressed rows already rejected by the normal reader are different.

Direct source/restored first returned `BodyContactSample` parity, excluded-row
coverage and malformed/invalidation checks are pending at this review point.
The initial focused guard fixture reports **29 checks / 3 failures** and is
preserved; it is not accepted as completed guard evidence. A selective
new-capture repair is causally justified after the split experiments, but cannot
retroactively reconstruct omitted numerical/actuator/publication state in the
original archive. The original failure remains a reported compatibility gap.

## Reviewed proposal signoff

The later reviewed proposal includes both guards. Its independent contact-sample
receipt compares **all 12 alive bodies before the first resumed Prepare**. Every
returned field is bitwise equal by durable entity identity, including both
directional views of the actual pad/pad row (impulse **0.19666458666324615**).
Point, normal, separation, support velocity and sleeping flags match; no support
row is invented. Query-only bodies retain their normal unavailable-sample
result. This establishes actual reader parity, not merely stored-ledger parity.

The corrected focused unit passes **40/0** and covers remapped canonical body
order, cadence change, pose/mass/settings invalidation, NaN/truncated/newer
records, genuine completed-empty versus unpublished state, omitted readable
contacts, and a missing companion world participant. The original **29/3**
receipt remains separate. Proper unchanged fresh-process write/read pass
**24/0 and 13/0**, and the earlier original controller check passes **119/0**.

The reviewed owned PhysicsWorld source hash is
`0a59fd11626f9099806f9eae67879125e257f688814249a701324abae6bb81ff`.
The four companion hashes are recorded in
`linux-reviewed-continuation-isolation.json`. This reviewer signs off the
selective new-capture correction for application and final combined Linux/native
Windows validation. The signoff does not claim those final reruns have already
passed. It does not claim old a106 continuation is repaired. No solver budget,
controller gain, threshold, delay, body pose, fluid code or protected evidence
change is required by this proposal, and no source/build/test was changed or run
by this reviewer.

The owning agent subsequently applied the five reviewed sources. Their current
hashes match the approved proposal exactly. The independent JSON retains the
three previously reviewed pre-repair source hashes as `priorReviewedSHA256`,
rather than concealing this intentional transition. All **71** current reviewed
fingerprints verify, and the original a106 save hash remains exact. Final
combined candidate checks are still the owning agents' responsibility.

## Documentation and inventory review (before final M70 follow-up)

At the owning agent's request, this reviewer made narrow clarity edits only in
`docs/ARCHITECTURE.md`, `docs/M77.md` and `docs/judasjs/saves.md`: the current
cold-boundary report is identified separately from frozen prior corrective
stages, guarded complete-compatible publication is distinguished from ordinary
reacquisition, and an omitted eligible positive contact retains unavailable
publication/first-wait semantics. Controller settings are unchanged; this does
not assert that the restored producer-phase code is unchanged.

The causal repair report does not claim the original a106 archive is repaired.
The old native source/guarded replay journal's seven final post-solve body states
are exactly equal after body-ID remapping at the first prepared boundary and
tenth iteration. The newly captured source/restored final observer JSON also
matches every recorded field after grouping its 12 bodies by durable entity
identity. These fixture-specific results support the measured exact-match
wording without creating a universal deterministic-continuation promise.

Read-only review of `probes/capture_final_inventory.py` found no substantive
allowlist/preservation defect. It checks frozen historical manifests, raw
evidence and unrelated local inputs; requires the starting HEAD and an empty
index; bounds selected task paths; rejects symlinks/deleted task files/build
outputs; and checks for changes during capture. Generated self-reference and
optional verification exclusions are explicit. Capture must wait for final
reports/results to freeze; the optional verification receipt should be listed
separately if produced after capture. This reviewer did not execute the probe.

At this review point the owning agent reports final Linux continuation **49/0**
and passing affected M77 checks, while a Character Lab cold-load **14/3** gap is
under investigation and final native Windows is pending. The source signoff
above is not acceptance of that unresolved combined candidate.


## M70 partial-anchor reconstruction review (initial copied probe)

The final combined Character Lab cold read exposed a separate pre-existing
physical-state capture gap. `RuntimeWorld::GetEntityState` returns cached
`e.state` for a slotless record, while partial physical-animation anchors are
ordinary static bodies moved directly through PhysicsWorld. The scene portion
of `WorldPersistence::Entities` captures the correct physical presentation pose,
but its later entity record can capture the cached initial pose. Restoration
then overwrites the correct reconstructed scene with that cached pose. The
slotless `SetEntityState` path also calls `ResetBody` without restoring linear
and angular velocities, so those actual prescribed-body velocities are lost.
The exact joint-context guard correctly exposes this mismatch rather than
relaxing the guard.

The copied first correction captures actual live physical state in the existing
entity record and restores actual body velocities. Its source/restored observer
records independently compare equal for all **37** bodies by durable identity;
every recorded position, quaternion, linear/angular velocity and motion type
match. Owning-agent receipts report unchanged M70 capture/read **10/0, 14/0**
and M77 **24/0, 13/0**. The first standalone minimal fixture retains one setup
assertion failure in both versions: it inspected the slotless entity record
before normal persistence preparation created that record. Its two actual
pose/velocity checks fail in the original and pass in the copied correction;
that receipt is not represented as an all-passing fixture.

The initial broad RuntimeBody override is not accepted unchanged. RuntimeBody
can also return a CharacterMotor observation capsule. That query pose includes
the authored local shape offset and is not the authoritative entity-root pose
or motor velocity. A Full-fidelity real-body/character exclusion must preserve
normal motor state. Reduced-fidelity retained state must also remain
authoritative. These guards and a nonzero-offset motor/reduced-fidelity proof
are pending at this review point; no new production-source signoff is implied.

Existing entities-v1 records remain readable, but old files which already lost
prescribed static velocity data cannot be promised retroactively exact. This
is distinct from the original a106 M77 missing-continuation-state gap.


## Final reviewed correction verdict

Controlled split experiments and the later bounded publication proof justify
a selective new-capture continuation contract: actual latched actuator commands,
finite-pass joint history, and genuinely completed readonly contact publication
are restored only under the reviewed physical/settings/mapping/timestep guards.
They are applied in the proper restored producer phase; solver equations, ten
passes, controller gains, deadlines, thresholds and task clocks are not retuned.
The original a106 file remains a failed **13/1** legacy continuation. Its omitted
fields cannot be uniquely reconstructed from its physical pose/velocity bytes;
the new-capture repair is not presented as a retroactive repair of that file.

The separate M70 follow-up corrects existing entity physical-record capture for
Full-fidelity real bodies, including slotless prescribed-static articulation
anchors. It retains Coarse/Dormant state authority and explicitly excludes motor
query proxies. Actual body velocities are restored in the existing entity
record. Independently reconstructed motor proxy geometry uses the authoritative
restored motor root plus its rotated authored offset, with authored enabled
state, layers/mask and tags set before any resumed callback. This is private
cold reconstruction; it does not modify general transform-teleport behaviour.
Joint/contact continuation is applied later, so these reconstruction writes
cannot invalidate legitimate already-restored continuation state.

The final copied proposal, WorldPersistence SHA-256
`39ae0eb2132f7b5f6f43710dd82accf38ef21b04dc1c57bd8af4279da84ad2bb`,
is signed off for application and final combined Linux/native Windows checks.
The new 49-check static/motor proof reports **49/10** on the frozen original,
**49/8** on the static-only correction and **49/0** on the full proposal. It
exercises arbitrary orientation, non-axis local offset, immediate enabled and
disabled queries, actual source rays, layers, tags and reduced-fidelity state.
The initial **49/2** overly strict cached-entity quaternion result is preserved:
the ordinary entity setter normalizes that cached quaternion again. Measured
component-L2 difference is **1.248e-7** (equivalent normalized rotation difference
about **2.036e-8 rad**); only this newly introduced cached-entity check uses
`<2e-7`. Actual motor/proxy quaternions and physical-body fields remain exact.
No historical acceptance test or threshold was changed.

Owning-agent copied application receipts pass unchanged M70 **10/0 + 14/0** and
M77 **24/0 + 13/0**. Observer records compare all **37** physical states equal
by durable identity, comprising 35 real bodies and two separately reconstructed
motor proxies. The earlier broad capture, fixture initialization failure, strict
context failure and offset-only intermediate result remain separate evidence.
The final combined production reruns remain the owning agents' responsibility;
this source signoff does not assert their completion.

No runtime source, test, protected research, old numerical evidence or old save
was edited by this reviewer. No build/test was run by this reviewer. Only the
explicitly authorized current documentation clarity edits and this owned
chronological independent record were written.

# Independent physics closure review — M78 corrective candidate

## Scope and identity

This is a fresh **read-only source review**, not a build or validation receipt. The reviewer did not edit runtime source, execute tests, or run a compiler. New proposed probes are stored beside this report for the parent to schedule. Earlier M78 evidence and the frozen independent cold-boundary report are preserved.

Starting HEAD remains `9e77b2226edcf217ab789fe4f0fea42a3415a72d`. The reviewed working tree is uncommitted. `REVIEW_SOURCE_IDENTITY.json` records the exact source bytes at this review boundary; later changes require their own follow-up receipt. Fluid implementations, fixtures, protected research and thresholds are outside this review.

## One high-risk combined-contract gap

### PHY-CLOSURE-001 — unrelated pre-physics motor refresh demotes genuine restored contact publication

**Status:** source-confirmed control-flow inconsistency; execution is still pending at this report boundary. Do not report the copied mixed-world fixture as passed or failed before it is actually run.

`PhysicsWorld::Impl::InvalidateRestoredContinuation(slot)` (`src/PhysicsWorld.cpp:265–274`) clears joint guesses only for joints involving `slot`, but then unconditionally clears the **world-wide** `restoredCompletedObservations` flag. `ResetBody` (`src/PhysicsWorld.cpp:2493`) invokes it even for an unrelated query-only motor capsule and an unchanged pose. The completed rows for other bodies remain readable and generation/revision-valid.

The normal production order makes this relevant: `StepPlayedWorld` invokes `UpdateCharacters(dt,true,false)` before `PreparePhysicalBalances` (`src/Simulation.cpp:35–38`). The former resets every participating M70 observation proxy (`src/WorldCharacter.cpp:38`). A restored M77 controller then sees no restored token and takes its old one-step unavailable-sample return (`src/PhysicalBalance.cpp:283–288`), skipping ordinary clocks and actuation despite genuine unchanged completed observations. A distant independent motor does not own the M77 skeleton, joints or support contact publication, but currently changes that first controller boundary.

The direct probe distinguishes two situations:

1. An unrelated query-only proxy reset should preserve the independent real publication and its readable rows.
2. Teleporting the body that owns a restored positive contact must invalidate that publication and suppress stale rows.

`RestoredPublicationIsolationProbe.cpp` covers these using actual positive solved contacts at the unchanged 60 Hz cadence. `MixedBalanceLabProbe.cpp` copies the ordinary M77 cold-write/cold-read driver and adds one distant scriptless M70 motor clone. Original M77 controls, timings, total resumed step count, thresholds and acceptance checks remain unchanged. The copied test logs first-boundary readiness and controller clock advancement. This is a combined-system regression check, not a changed historical fixture.

A repair, if reproduced, should scope publication invalidation to affected physical observation/continuation owners while preserving real body/joint invalidations. Do not remove invalidation wholesale, fabricate known-empty observations, advance controller clocks by exception, or special-case a scene or actor.

## Reviewed ownership and numerical seams

| Area | Source trace and current conclusion | Remaining qualification |
|---|---|---|
| Free angular motion | `RigidBody.cpp:28–65` solves body momentum at implicit midpoint; `IntegrateRigidBodyPosition` transports world angular momentum into endpoint inertia. `RigidMotion.h:96–104` freezes the geometric generator at the segment anchor. | Torque kicks remain first-order splitting; shortened anchored prefixes are not exact new midpoint substeps. Explicit bounded nonconvergence throws rather than silently replacing the integrator. |
| TOI physical state versus trajectory | `PhysicsWorld.cpp:444–453,504–510,858–867,954–965` samples the right event limit and physical transported angular velocity, while CCD speed/plane certificates use the drift generator. `ChangeVelocity` starts a new segment only after an actual solved velocity change. | Full world angular-momentum proof is for appropriate closed-system fixtures; positional penetration correction is not a claim of universal mechanical conservation. |
| CCD mesh enclosure | `PhysicsWorld.cpp:534–546` uses all eight immutable mesh AABB corners solely as a separating-plane certificate. If the enclosure does not certify separation, the prior conservative/sampling search remains. | This broad bound is not contact geometry and does not justify reducing contact fidelity or CCD requirements. |
| Boolean contact existence | `CollisionGeometry.cpp:114–132,184–204` shares SAT, sidedness, clipping and witness acceptance with full manifold generation. CCD may stop at the first accepted witness; actual response still calls the complete manifold (`PhysicsWorld.cpp:881–894`). | Unsupported mesh/mesh pairs have a pre-existing dispatcher limitation; production concavity is static-only and `KinematicShape` excludes meshes, so this review does not treat that unsupported direct call as a new retained correction. |
| Completed contact observations | Separate completed rows survive unrelated warm-cache removal. Actual TOI primitive groups replace only their own solved rows; other stance rows survive (`PhysicsWorld.cpp:895–914`). `ReadBodyContactSample` validates current generations/revisions/filtering, bounds contacts/candidates, and fails closed on overflow. | The value is a completed solver observation, not a new current narrowphase or a force sensor. The restored-token global invalidation gap above remains. |
| Joint continuation | Actual latched angular-drive commands and 12+3 selected finite-pass numerical impulses are guarded by body/settings context and solver-format/dt (`PhysicsWorld.cpp:2825–2849`). Different resumed dt discards guesses, keeps valid intent (`2067–2077`). | Context does not promise identical arbitrary reordered joint/global solve problems. Original legacy `a106…` cannot recover data absent from its archive and remains failed. |
| Contact restoration | `PersistCompletedContactObservations` (`2860–2923`) validates bounded finite data, immutable physical context, durable mapping, current filters and primitive indices; canonical remapping flips normals/anchors correctly. An omitted eligible transient row demotes the entire archived publication. | No warm-contact cache population, impulse replay or event replay. A genuine completed-empty epoch differs from unavailable publication. |
| Static versus reduced state | `WorldPersistence.cpp:47–60` captures actual present Full rigid-body pose/velocities, including prescribed static anchors, but preserves reduced-state ownership and excludes motor proxies. | The old archive cannot retroactively recover missing prescribed state. Full real-body state is not silently replaced by an inactive reduced proxy. |
| Motor restoration | `WorldPersistence.cpp:65–80` reconstructs query capsule from restored root plus oriented shape offset, enabled state, masks and tags before restored contact publication and callbacks. | This is a private cold-reconstruction fix, not a redesign of live `SetRuntimeTransform` or motor root ownership. |

## Failure, malformed-data and lifetime checks

The current continuation code reads records into local data before installing a validated joint record or completed-observation vector. `SaveArchive` rejects nonfinite numbers, invalid booleans, degenerate quaternions, truncated data and bounded count/string excess. Joint drive restoration adds a near-unit target guard. Required participant version pairing prevents new articulation context silently losing its corresponding observation context.

A multi-participant `WorldPersistence::Restore` can modify its destination before a later participant fails; **that destination is the detached private candidate** in `SaveService.cpp:128–137`. The service destroys it on failure and does not publish a partial live world. This contract must not be rewritten as transactional rollback of any arbitrary caller-supplied live `RuntimeWorld`.

Destroyed/reused body handles are checked by generation. Context and mapping checks reject mismatched reconstructed bodies/settings before continuation use. Mutation invalidations for pose, enable state, filters, sensors, material, mass/inertia, pair suppression and motion authority remain important; the finding is excessive invalidation of unrelated publication, not absence of these guards.

Ordinary velocity, force, impulse and unchanged drive submissions deliberately retain normal prior-step numerical policy. Clearing restored guesses on every intentional next-step input would itself diverge from uninterrupted source semantics; no such change is proposed here.

## Existing evidence and limits retained

- Frozen M77 `a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b` legacy snapshot remains **13 checks / 1 failure**. New-format continuation successes must not be described as repairing absent bytes.
- Earlier hinge warm-start predictive row-topology dependence is a real separately recorded numerical seam, but has no active limit row at the first exact M77 divergence. It is not a justification for an unrelated JointSolver rewrite here.
- Existing all-boundary converged box-QP experiments did not repair the nonlinear controller trajectory; smaller subproblem residual is not evidence of better physical control.
- Four-ragdoll host p95 improvements, remaining tails and incomplete mesh-rig sleeping must retain exact workload/quality and platform labels. Windows VM wall time is not hardware/GPU certification.
- FLU-001 remains failed on Linux under its unchanged criterion. This review neither changes nor recertifies fluid.

## Recommended bounded closure checks

1. Schedule the two supplied isolation/mixed-world probes against the frozen candidate before a repair; preserve failure output and exact source identity.
2. If a narrow publication-ownership repair is justified, repeat the same probes unchanged, plus existing 98-check continuation guards, M70 and M77 separate-process cold cases and first actual returned-contact parity.
3. Preserve negative same-pose/teleport, disabled/sensor/filter/pair/material/mass and changed-dt checks. Check an unrelated proxy destroy/disable path if it shares the repaired helper.
4. Retain existing angular, contact-lifecycle, CCD, Boolean-manifold equivalence and static/reduced/motor-offset checks. Do not infer new passes from this read-only review.

## Artifacts

- `RestoredPublicationIsolationProbe.cpp` — minimal proposed reproduction.
- `MixedBalanceLabProbe.cpp` — original M77 test plus a distant independent M70 motor.
- `PROBE_IDENTITY.json` — source/probe identity when those probes were written.
- `REVIEW_SOURCE_IDENTITY.json` — reviewed source identity and review status.

No builds, tests, commits, push or tags were performed by this reviewer.

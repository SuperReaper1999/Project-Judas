# Independent review: restored corrective-step first phase

Read-only review of the continuing uncommitted M78 candidate. No build, simulation, production edit, fixture change, or final fingerprint capture was performed. Source/receipt hashes and exact selected trace rows are in [the companion JSON](reviewer-cold-load-first-step.json); these identify this review point, not the eventually combined final candidate.

## Confirmed discontinuities

- `PhysicalBalanceController::PersistHistory` restores the complete bounded semantic task history but deliberately clears its observation and marks `m_waitingForSample`.
- The first restored `Prepare` returns before `elapsed`, `referenceTime`, `postStepTime`, `swingTime`, and `transferTime` advance and before targets/drives are evaluated. This is more than a contact-observation gate.
- In the exact native saved mid-swing example, the uninterrupted first frame advances `swingTime` from 0.283333361 to 0.300000042 seconds. The restored first frame leaves it at 0.283333361 and clears directly measurable COM/gravity-derived attitude along with missing contacts. Both uninterrupted and restored first-frame contact counts are `[0,0]` in this particular fixture.
- Recreated joints omit semantic runtime angular-drive commands as well as disposable solver impulses. Because physical balance already owns the joints, `PreparePhysicalAnimations` correctly leaves their actuation alone. The guard therefore produces a first physical step with disabled drives unless a command replay is supplied.
- `PersistTouches` restores event-pair Stay/Exit history, not completed per-point solver observations. `ReadBodyContactSample` returns true with an empty vector both for a real completed zero-contact step and for a reconstructed world without any completed sample. Thus the bypass has no public readiness distinction.

## Isolation result and meaning

The unchanged-snapshot command-only overlay still failed. The first-wait bypass alone also failed (the parent reported zero landings against one in its paired baseline). Neither is a sufficient correction, and this review does not assign the entire failure to the guard or omitted command. The uninterrupted trajectory from the exact saved state succeeded; cold-load robustness remains a real issue. Disposable warm history and numerical ordering are still under independent causal investigation.

## Risks in a blanket bypass

With unpublished contacts treated as genuine absence, ordinary Prepare may reset `settled`, withdraw a supported Succeeded state, or infer `lifted` from absence. It can also recompute the carrier/frame without a completed load sample. Active swing references use saved planned/stance body identities and current world transforms, but existence of a body is not evidence of a new contact. A bypass must not synthesize positive supports, loads, touchdowns, or accepted standing.

## Smallest defensible seam to investigate

1. Treat angular-drive enabled/target/stiffness/damping/maximum torque/relative angular velocity as semantic command data; exclude `JointAngularDrive::impulses`, which `JointSolver::Prepare(warm=true)` uses as disposable numerical warm starts.
2. If commands are restored, do so after balance Configure/history recreation (Configure rejects pre-owned enabled drives) and before sleep state (setting a drive may wake bodies). Resolve only current durable bodies/constraints and validate ordinary bounds.
3. Separate contact readiness from zero contact. If first-step reference progression is changed, keep the normal dt for independent timeline evaluation while gating only decisions that require a completed contact sample. Pending/unsupported must not be recast as a contact claim.
4. Do not retain this design merely because it passes one snapshot. The combined isolation must establish the cause and preserve existing standing/recovery, arbitrary gravity, disabled/stale support, no-effort, teardown, and cold-save contracts.

Gravity itself is sampled per mapped body through `SampleEntityGravity` in `PreparePhysicalBalances`, matching normal dynamic-body integration. No universal-up or owner-gravity shortcut was found here. M61 explicitly excludes constraint warm starts and promises bounded continuation, not bit-identical indefinite physics; restoring warm impulses solely to reproduce a passing trajectory would change that contract and is not this review's recommendation.

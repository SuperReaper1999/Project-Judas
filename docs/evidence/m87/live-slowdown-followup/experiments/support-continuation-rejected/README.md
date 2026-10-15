# Isolated M87 persistent-support continuation experiment

This is a numerical-policy experiment, not production code or an exact optimization. Shared repository source, build products, assets, settings and protected evidence are untouched. The frozen source/prototype hashes are in `provenance.json`; `support-continuation.diff` is the complete experimental change.

## Hypothesis

The fixed-step persistent solver accepts a previously matched support with a small positive gap and limits its closing speed. A later exact impact can invalidate that gap-closing constraint. `AdvanceImpacts` correctly drops that support exemption rather than assume a changed trajectory is still safe. The current actual-contact-only event island cannot resolve the separated support, so its subsequent first touch can cause another event, another invalidation and another search. Many such transitions might contribute to the measured dense-pile event cascade.

This experiment refreshes the already-existing support component at each true event and reapplies its remaining-time normal constraints. It aims to reduce repeated first-touch events. It does not assume that this is the demonstrated cause, or claim that fewer events are sufficient proof of correct behaviour.

## Mechanism and scope

1. Remember only support groups already accepted by the fixed-step solver and later inelastic actual events, with full body generations and real local anchors/normals.
2. Discover from the actual event component through those existing support groups and ordinary active joints. Fresh signed-gap manifolds must match the current engine's existing warm-anchor distance and normal-dot criteria. No new positive-gap pair becomes a support merely because it is nearby.
3. Expand through actual zero-margin contacts of newly reached support members. Discovery is read-only; uncertain geometry, newly reached sleeping/deferred-acceleration bodies, out-of-range arithmetic and moving infinite-mass boundaries retain the original path.
4. Run the original `ImpactSolver` and publish its observations unchanged. It still receives only actual contacts.
5. Separately project all current actual rows plus matched prior supports through a normal-only `ContactSolver`, with zero warm impulses and explicit target `b = -max(gap, 0)/(dt-now)`. Current actual rows have target zero. Use the existing iteration count. No positive-gap friction, restitution, extra gravity, spring/motor/constitutive reintegration or position correction is introduced.
6. Hard joint rows use `impact=true`, zero warm state; existing `JointSolver` therefore excludes spring, motor, drive, viscous-resistance and finite-material efforts. Complete physical states and joint observations are snapshotted before this projection and restored on the energy guard's rejection.
7. Keep **all original ordinary CCD invalidation, rescheduling, spinning support handling, actual-event support insertion and caps unchanged**. The refreshed positive-gap rows gain no CCD exemption. In particular the linearized gap constraint is not asserted to enclose a rotating shape's complete nonlinear trajectory.
8. Positive-gap rows never publish collision/contact callbacks or completed actual-contact observations. Accepted extra normal impulses on real actual rows are recorded separately without replaying the actual-impact impulses.

The prototype uses a clearly marked `const_cast` on the actually non-const local solver only to set the experiment's explicit targets. A production design would need a small reviewed internal solver option. No public binding or API is added by this isolated experiment.

## Mechanical and numerical reasoning

For frozen event-time normal Jacobian `J`, inverse mass/inertia `M^-1`, post-impact velocity `v0`, and targets `b_i <= 0`, normal constraints require `Jv >= b`, lambda >= 0. With zero initial lambda, accumulated projected Gauss-Seidel is coordinate descent on

    F(lambda) = 1/2 lambda^T (J M^-1 J^T) lambda + lambda^T (J v0 - b).

At exact arithmetic each coordinate update cannot increase F from its initial zero. The kinetic change is

    E(v)-E(v0) = F(lambda) + lambda^T b <= 0.

Homogeneous hard-joint rows fit the same projection. Equal/opposite impulses at the fresh common anchor preserve linear and angular momentum, except for explicitly measured external fixed-boundary reaction. These arguments are local linearized statements, not a proof of full nonlinear trajectory separation or finite-precision correctness.

The same existing impact energy tolerance is used as a diagnostic acceptance guard: 64 binary32 epsilons times max(1, before kinetic energy). On guard failure, the complete post-impact snapshot is restored and the unchanged CCD path continues. The prototype records worst energy rise, momentum/angular residual after fixed-boundary reactions, and final normal-velocity violation. A nonzero residual/violation must be reported, not hidden by test retuning. Moving external anchors are skipped because the prototype does not claim their full work-accounting contract.

The current `ContactSolver::Prepare(h)` is not sufficient by itself: a separated row above the restitution speed threshold takes the `reaches && bounces` branch and receives zero target when restitution is zero. The explicit target override avoids silently turning this experiment into zero-gap clamping.

## Private controls and reproduction

- Default: experiment enabled.
- Presence of `JUDAS_SUPPORT_CONTINUATION_ORIGINAL`: same binary, original solver path.
- Presence of `JUDAS_SUPPORT_CONTINUATION_TRACE`: one diagnostic record per fixed step, including attempts, accepted projections, rows, positive-gap rows, added component members, rollbacks, skipped cases, budget residuals, normal violation, events and queries. Disable trace for final timing.

Build only when the parent has released the CPU:

    python3 /tmp/m87-support-continuation/build.py > /tmp/m87-support-continuation/build.log 2>&1

The script compiles one frozen translation unit and links it before the current engine archive into `/tmp/m87-support-continuation/live`. It reuses the existing observer/application objects and adds the current `libjudas_jobs.a` when present. It never overwrites the shared archive or executable.

Use the parent's exact existing 155-step project/input/frame-cadence commands for before/after. Unlike prior exact optimizations, changed trajectories are expected and must be examined against unchanged physical acceptance conditions. A favourable event count alone is not acceptance.

## Required acceptance if the event-count hypothesis succeeds

- True isolated-impact/restitution law and high-speed/thin-wall CCD remain passing.
- Ordinary persistent gap closure, a disturbed three/five-body stack and actual touching chains remain physically sensible.
- An unrelated disjoint component remains unchanged.
- Arbitrary rotations of the fixture rotate the response equivalently.
- Normal and tangential physical momentum/energy budgets, finite state and actual penetration remain acceptable.
- Hard joints retain anchors; no motor/spring force is applied an extra time.
- Sleeping, moving boundaries and uncertain cases demonstrably fall back to the original path.
- No positive-gap collision event is emitted.
- Cold-save continuation, original M70/M77 and relevant articulation/impact regressions pass without changing thresholds.
- The original dense fixture keeps all objects, materials, geometry, cadence and CCD quality; measure complete fixed steps including projection/discovery overhead and resource use.

No runtime acceptance is claimed by preparing these files.

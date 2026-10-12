# M77 corrective-step mechanism isolation

Investigation artifacts only. No original M77 fixture, input, timestep, gain,
geometry or success threshold was changed. The unchanged fixture SHA-256 is
`fabaca3fea9087db521c6a43fcb45dd3731dd621c33e328a15ff247366afd772`.

## Current conclusion after the actual-application follow-up

Three generic physics seams were isolated. Dynamic angular velocity must be
transported to the actual TOI orientation, physical post-impulse evaluation
must use the right limit at shared segment times, and completed contact
observations must not be owned by invalidated warm-start history. Correcting
those seams passed the unchanged 119-check Linux C++ controller fixture, but
the actual authored GL/JS application still failed its unchanged 15-check
corrective-step case. That failure remains in `combined-balance-step.log`.

The final isolated integration correction solves the body Euler equation by
implicit midpoint and uses the corresponding Cayley rotation for the existing
anchored drift. This preserves both world angular momentum and kinetic energy
at a full free-drift horizon, within represented arithmetic, whereas the
explicit gyroscopic predictor followed by momentum transport changes energy.
No controller implementation, authored settings, fixture or acceptance
criterion was changed. See [the mathematical and consumed-segment proof](M77_ANGULAR_MIDPOINT_PROOF.md)
for the important shortened-TOI interpolation limitation.

| Actual unchanged Linux application archive | Checks / failures | Linux C++ fixture |
|---|---:|---:|
| frozen original physics | 15 / 0 | 119 / 0 |
| completed observations only | 15 / 1 | 119 / 0 |
| right-limit + completed observations | 15 / 1 | 119 / 0 |
| right-limit + observations + planted-foot endpoint diagnostic | 15 / 1 | 119 / 2 |
| right-limit + observations + RK4 endpoint diagnostic | 15 / 0 | 119 / 2 |
| right-limit + observations + implicit midpoint | **15 / 0** | **119 / 0** |

The foot-reference and RK4 diagnostics are not retained. Removing gyroscopic
motion also made the actual application pass, but violates the independently
verified angular-momentum contract and is not a fix. Transport-only,
predictor-only and right-limit transport-only actual application variants all
failed. Their original results are preserved.

On the combined production candidate, `midpoint-final-controller.log` records
119 / 0, `midpoint-final-application-step.log` records 15 / 0,
`midpoint-final-motion.log` records 10,239 passing motion checks, and
`midpoint-final-contact_lifecycle.log` records 1,076 / 0. Final broader and
native Windows results are reported by the parent continuation report.

## First isolation: support observation ownership

The following records the first Linux C++ fixture isolation. Its pass alone was
not sufficient for the actual application, as the later section above shows.

In the failing candidate, `ReadBodyContactSample` observed `contactCache`, which
was solver warm-start storage. `AdvanceImpacts` invalidated that storage for every body in
an affected articulated island after a real time-of-impact solve. A newly
landing foot could therefore erase completed load-bearing observations belonging
to the other foot. The actual impact solver's contact rows were not published
to that observation source. This was an observation/solver-history ownership
defect; it is not insufficient M77 controller effort.

The isolated `candidate_observations` changes only this observation seam and
passes all **119 unchanged checks**. The physically corrected
`right_observations_v2` also passes all **119 unchanged checks**. Neither
changes the warm-start invalidation, contact solve, constraint gains, cadence
or step reach. Original Linux/Windows failures remain in their original
evidence; the failed variants below remain preserved.

| Isolated variant | Unchanged checks | Interpretation |
|---|---:|---|
| frozen physics | 119 / 0 failures | Passing original physical implementation |
| candidate | 119 / 2 failures | Recorded regression |
| no gyroscopic correction | 119 / 0 failures | Bisect only; independently violates angular momentum |
| angular transport without predictor | 119 / 1 failure | Does not satisfy unchanged fixture |
| predictor without angular transport | 119 / 2 failures | Does not satisfy unchanged fixture |
| right-limit event transport | 119 / 4 failures | Corrects independent physical TOI error; insufficient for M77 |
| right-limit + transport without predictor | 119 / 2 failures | Insufficient |
| independently converged RK4 drift endpoint, candidate | 119 / 3 failures | Drift accuracy alone is not the cause |
| independently converged RK4 drift endpoint, right-limit | 119 / 2 failures | Drift accuracy alone is not the cause |
| energy-preserving implicit Euler midpoint, candidate | 119 / 2 failures | Energy accuracy alone is not the cause |
| energy-preserving implicit Euler midpoint, right-limit | 119 / 4 failures | Energy accuracy alone is not the cause |
| completed observations only | **119 / 0 failures** | Sufficient generic observation correction |
| right-limit + completed observations | **119 / 0 failures** | Preserves both physical angular transport and controller evidence |

At this first isolation stage the alternative integrators were diagnostics
only; the later combined application isolation above established the retained
midpoint correction with an independent mathematical invariant proof. No
numerical method is selected merely for a favorable pass result.

## Independent support fixture

`probes/completed_support_observer.cpp` uses only ordinary PhysicsWorld shapes:
a settled dynamic box on static ground and an incoming sphere. It has no
PhysicalBalance controller, animation, scripted gains or acceptance retuning.

Before the incoming impact both implementations report four floor rows and
0.654000506 N s normal impulse. After one actual TOI, the original source
reports zero floor rows, whereas completed observations report two actual
solved floor rows and 24.1254768 N s. The physical results are identical:
one impact, box y = 0.511879802 m and vy = -2.70025325 m/s. This proves the
correction changes observations rather than fabricating force or geometry.
Disabling/re-enabling the observed body and destroying/reusing its support
slot both invalidate the sample as required.

The observation ledger snapshots the existing completed start solve and
replaces only actual TOI primitive groups with the latest solved rows.
Untouched stance groups remain available. Repeated TOI loads are not summed
or duplicated. Existing body generations, observation revisions, bilateral
filters, sensor/query-only exclusion, broadphase candidate limits, contact
limits, normal/gap/anchor semantics and false-on-overflow behavior remain.
The existing accepted start solver's gap-closing rows are not newly invented
or broadened. New TOI rows come from actual zero-margin geometry and
`ImpactSolver::Constraints()`.

Production integration must also clear this store in `SetCollisionFilter` and
`SetPhysicalMaterial`, alongside their existing explicit warm/pending cache invalidation. This preserves
the original exclude-then-restore-before-a-step behavior; the diagnostic
variant was sufficient to isolate the solver-observation cause, and the
additional invalidation belongs in the final safety checks.

The final focused tests should cover actual incoming body/floor impacts,
untouched stance rows in a joint island, repeated TOI primitive-group
replacement without duplicated loads, disabled/sensor/suppressed bodies,
filter/material changes before the next step, destroyed/reused handles, reset
revisions, contact/candidate overflow, and save/reload sample reacquisition.
The companion right-limit motion test should preserve existing left-limit
checks while covering same-time multiple impulses, zero-length segments,
an event exactly at the step end, adjacent representable times, empty motion,
out-of-range and nonfinite queries. Neither observer changes controller rules.

## Independent angular-impact correction

`probes/event_angular_transport.cpp` separately demonstrates that advancing
orientation to a TOI while retaining the earlier-pose angular velocity loses
world angular momentum for an anisotropic body. Original candidate impact
error: 0.040292114 kg m2/s, relative 0.0048120321. Right-limit event transport:
1.50055871e-7 kg m2/s, relative 1.79209676e-8. This correction is physically
required independently of whether M77 passes. Original geometry/history
left-limit segment semantics remain distinct from post-impulse physical
right-limit state.

## Corrective-step phase observations

The unchanged allowed trajectory with right-limit + completed observations
lands at step 83, retains physical support, and finishes at root y =
1.38095999 m, upright = 0.999840319, speed = 0, one landing. With completed
observations only it lands at step 59 and finishes at root y = 1.36478174 m,
upright = 0.998359919, speed = 0. Both are accepted by the existing assertions.
All 720-step traces and standalone fixture logs are preserved alongside
source/library/executable fingerprints.

## Reproduction

These scripts use the existing clean Release object/link recipes from
`.cache/m78-followup/release`, creating owned archives beneath
`.cache/m78-continuation/variants`. They refuse to overwrite prior output.

```sh
python3 docs/evidence/m78-continuation/probes/contact_observation_bisect.py candidate_observations:candidate
python3 docs/evidence/m78-continuation/probes/contact_observation_bisect.py right_observations_v2:event_transport_right
python3 docs/evidence/m78-continuation/probes/link_angular_observer.py completed_support_observer candidate right_observations_v2
python3 docs/evidence/m78-continuation/probes/link_angular_observer.py step_phase_observer candidate_observations right_observations_v2
```

For reruns, choose fresh variant/output names; preserved outputs intentionally
cause these exact commands to refuse overwriting existing evidence. Acceptance
must still be established on the combined final production candidate on Linux
and Windows; isolated results do not replace that validation.

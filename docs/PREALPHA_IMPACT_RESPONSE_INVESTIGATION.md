# Dense impacts: repeated resting-contact projection

## Result and scope

**The core scaling failure remains unresolved. No runtime correction from this
investigation is adopted.** This is a causal investigation and rejected prototype,
not another performance completion claim.

Checkpoint: `6059bafb43be4650162d9a6bdec05ea027135abc`, Experimental. Main remains
`3ecf26cdf248e271c96d17928bd33634e37f30ce`. The previous
[event-dependency checkpoint](PREALPHA_EVENT_DEPENDENCIES.md) removed scheduling
waste; it did not remove the repeated connected response described here.

The original `room1024.judas` is unchanged: 1,024 loose pieces, 1,285 total bodies,
96 joints, 60 Hz physics and eight maximum catch-up steps. Linux host: Ryzen 5
5600, 8 GB RAM, GTX 970; native Release/GNU 15.2. No Windows or Android claim.

[Raw evidence and reproduction](evidence/prealpha-impact-response-20261021/README.md).

## Actual mechanism

1. [AdvanceImpacts](../src/physics/WorldImpactEvents.cpp) grows the whole actual
   touching-contact/joint island at each selected arrival. In the dense pile,
   hundreds of bodies really are connected; this is not evidence of an erroneous
   graph union.
2. [ImpactSolver::Solve](../src/ImpactSolver.cpp) clears its constraint solve and
   prepares every contact with zero starting impact impulses. It then repeats
   the ordinary ten velocity sweeps, including all pre-existing contacts.
3. [ContactSolver::SolveVelocities](../src/ContactSolver.cpp) therefore corrects
   residual closing/slipping from the resting-contact problem as well as the
   newly selected collision. A finite ten-sweep solve is not an exact projection:
   calling it again can change velocities without any new impact.
4. Every exact velocity change replaces a motion segment. The event scheduler
   correctly invalidates dependent searches. Hundreds of changed trajectories
   therefore cause thousands of fresh searches, even when most again miss.
5. The previously preserved Float-motion investigation additionally establishes
   near-tangent arrivals caused by represented-pose jumps. Such a small arrival
   can trigger another whole-island projection. Predicate results are correct
   for the supplied Float poses; changing just one query to double would not
   make motion, response and contact lifetime consistent.

This explains why a faster queue or an identical-input miss cache cannot alone
resolve the problem: the globally changed trajectories are genuinely different
inputs under the current response algorithm.

## Counterfactual proof using actual game states

A read-only observer captured ten events in step 347. All 347 trajectory digests,
physical observations and work counts match the unmodified checkpoint, with zero
mismatches. Independent offline clones reproduce the native post-solve linear
and angular velocities bit-for-bit in all ten cases. The observer performs no
counterfactual solves in the measured world's execution.

For each clone, a second solve removes **only** the selected primitive's contact
rows. All other bodies, velocities, geometry, materials, ordering and solver
settings remain unchanged. Counts below include bodies referenced by contact
rows, including static boundaries; they are not all dynamic island counts.

| Step 347 event | Contact rows | Referenced bodies | Bodies changed by full solve | Changed identically with selected rows removed |
|---|---:|---:|---:|---:|
| 9 | 2,475 | 936 | 854 | 225 |
| 10 | 2,474 | 936 | 848 | 250 |
| 13 | 2,482 | 940 | 816 | 293 |
| 18 | 2,474 | 937 | 810 | 807 |
| 21 | 2,489 | 945 | 812 | 803 |
| 25 | 2,475 | 937 | 784 | 781 |
| 28 | 2,490 | 946 | 751 | 751 |
| 35 | 2,487 | 941 | 714 | 710 |
| 38 | 2,486 | 940 | 644 | 640 |
| 58 | 2,460 | 938 | 619 | 614 |

**Event 28 is decisive:** the selected contact is separating, receives zero
normal impulse and changes no bodies when solved alone. Removing it leaves all
751 velocity changes exactly unchanged. They are caused by re-solving other
contacts, not by this arrival.

For event 18, the selected contact receives about `1.9891e-6 N s`; 807 of the 810
changed body results are bit-identical with that contact removed. Repeating the
full projection on the resulting state with no new impact changes another 784
bodies. Maximum contact closing speed drops from `0.0009306364 m/s` before the
first solve to `0.0007326150 m/s`, then `0.0004986407 m/s` after the second. This
is repeated residual correction, not evidence of energy injection.

Zero-velocity controls change zero bodies in every captured case. Every tested
native impact energy guard passes. These ten cases establish a concrete
mechanism; they do not classify all impacts or prove what fraction of every
scenario's total cost has that cause.

## Why the obvious replacement is unsafe

A new-impact-only solve is insufficient: an independent three-body chain test
requires the impact to reach the third body, with final speed approximately
`1/3 m/s` for all equal masses. Cutting the contact island would miss that.

Nor can the old resting correction simply be subtracted. For a frictionless,
unit-mass line with incoming velocities `(1, 0, -1)`:

```text
projection using only old B-C contact: (1, -1/2, -1/2)
projection using both contacts:       (0, 0, 0)
input + full_projection - old_projection = (0, 1/2, -1/2)
```

The proposed difference response leaves B closing into C at **1 m/s**. The native
ten-sweep replay produces that same 1 m/s closing violation. Energy decreases,
so an energy guard alone does not detect it. This candidate is rejected.

Carrying previous impact impulses forward blindly is also unjustified: they
represent impulses already physically applied at an earlier event. Reapplying
can double-count them; permitting their release at a later event changes past
impulse ownership. A valid persistent/incremental formulation must define the
free state, constraint lifetime and reaction history explicitly.

## Rejected dispatch-time certificate prototype

A private overlay records previously completed, uncoupled inelastic contact
pairs. Before dispatching a repeated pair, it retries the **existing** continuous
support certificate on the current trajectories and remaining interval. It
changes no certificate tolerance or iteration budget; it does not skip failed
proofs. No production file contains this overlay.

The unchanged 400-step fixture was run serially for both binaries:

| Measurement | Current checkpoint | Prototype |
|---|---:|---:|
| Mean step | 67.348 ms | 67.353 ms |
| Median step | 31.178 ms | 31.403 ms |
| p95 step | 201.964 ms | 227.950 ms |
| Worst step | 533.842 ms | 324.604 ms |
| Impact events | 4,835 | 4,780 |
| CCD queries | 3,171,426 | 3,135,313 |
| Search iterations | 2,399,326 | 2,390,836 |
| Maximum RSS | 179,424 KiB | 177,988 KiB |
| Endpoint native maximum penetration | 6.598 mm | 21.614 mm |

Only 19 dispatches were directly certified. Chronology/trajectories diverge from
step 153, so the 55 fewer eventual events are not 55 direct proof acceptances.
The lower single worst step does not establish a robust improvement; average
cost is unchanged, p95 worsens and the sampled endpoint is worse. No trajectory
or physical-accuracy equivalence is claimed. Prototype excluded.

Two additional serial **visible** 35-second runs reproduce the actual stall:

| Measurement | Current checkpoint | Prototype |
|---|---:|---:|
| Late output cadence, frame endings at 10–35 s | 0.969 FPS | 1.035 FPS |
| Worst late frame | 1.833 s | 1.837 s |
| Completed physics steps | 489 | 505 |
| Simulated seconds per wall second | 0.232 | 0.238 |

The wall windows reach different simulation endpoints; these are live-cadence
observations, not matched-trajectory accuracy tests. Early fast frames make the
whole-run mean approximately 10 FPS and the median frame misleading. Neither
binary meets the requested practical target. No long-run qualification or broad
regression run was spent on this rejected prototype.

## Repair boundary and remaining work

The unresolved engineering task is to distinguish sustained-contact error
correction from a new impact's physical response, while preserving real impulse
propagation, friction, unilateral contact and motion-continuation validity.
Persistent constraint state and incremental impact processing are plausible
architectural directions, not an implemented or validated solution here. Contact
geometry/lifetime must also agree with the represented/continuous motion model.
A different coupled solver could be viable; this investigation does not prove
one unique architecture necessary or that a repair is impossible.

A future candidate must demonstrate that a zero-effect arrival cannot cause an
unrelated whole-pile solve, that real chain impacts still propagate, that finite
feature departure/recontact remains detected, and that conservation, penetration,
sleep/wake, joints, curved-mesh CCD and cold saves retain their contracts. It must
then improve matched short/long and live dense workloads. Arbitrary island caps,
impulse thresholds, stale-miss reuse, forced sleep and reduced cadence are not
substitutes.

**No safe working repair has been established in this investigation.** The
blocker is numerical contact/impact ownership and continuation, rather than a
missing thread or an isolated expensive triangle routine. This follow-up leaves
production source and binaries unchanged; new report/evidence remains uncommitted.
Main, protected historical artifacts, game assets and independent research are
untouched. No M100 or release claim.

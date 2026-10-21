# Retained ordinary-support ownership review

## Conclusion and smallest correction

The source supports the narrow correction already tested by root: inside the
existing `continuationEligible` branch, retain the **ordinary fixed-step solver's
guesses**, rather than zeroing them merely because an impact changed endpoint
velocities. Preserve original generation checks, append only zero-load identity
for new TOI primitive groups, retain the existing full invalidation outside this
branch, and never copy event impulses into the ordinary warm cache.

No persistent geometry graph, new contact identity table, cache serialization or
new public API is required for this ownership correction. Existing
`ContactKey`/normal/local-anchor matching and solver-owned frame reconstruction
already supply the next-step query and numerical ownership boundary. A retained
guess is not a claim that its old contact geometry or impulse remains the actual
current load.

## Actual ordinary warm behavior

`src/PhysicsWorld.cpp:214–288` computes a fresh manifold, matches unused cached
points by both endpoint generations and primitive identity, world normal dot
threshold and local-A anchor distance, and supplies the old normal/tangent values
to new rows. New unsupported separated pairs retain normal TOI ownership.
The thresholds remain the original 0.05 m / 0.9 dot values; no tuning is proposed.

`src/ContactSolver.cpp:166–240` rebuilds offsets/effective masses from actual
current bodies and fresh contacts. It reads restitution targets before warm
application, projects the cached tangent onto the current tangent plane, clamps
it inside the current Coulomb disc, applies an initial numerical guess, then
performs the existing ten finite velocity sweeps. This is existing numerical
behavior; an old guess is not itself guaranteed optimal or energetically harmless
without the subsequent solve and physical regressions.

`src/PhysicsWorld.cpp:397–426` publishes the ordinary solve's resulting impulses
before `AdvanceImpacts`. `WorldImpactEvents.cpp` separately publishes actual
event observations into `completedContactObservations`. The continuation change
retains the former estimates and does not add the latter event impulses to them.
The new TOI groups still begin with zero ordinary load.

Eligibility remains restricted to zero effective restitution, no prepared hard
joint solver, no external velocity coupling, and native free Box/Sphere event
pairs. Existing elastic, joint/coupler and unsupported geometry invalidation
remains intact. It must not be generalized from the pile result alone.

## Lifecycle boundaries actually found in source

| Operation | Existing behavior relevant to retained guesses |
|---|---|
| Destroy/reuse body slot | `WorldBodies.cpp:107–145` increments generation on reuse; ordinary matching and sleeping graph reject old generations. |
| Collision mask/layer change | `WorldBodies.cpp:879–899` wakes and explicitly removes incident warm/pending/completed rows. |
| Physical material change | `WorldBodies.cpp:1012–1034` explicitly removes incident warm/pending/completed rows. |
| Motion-authority change | `WorldBodies.cpp:429–517` reconstructs mass/COM and explicitly drops old warm/observations. |
| Static collision replacement | `WorldBodies.cpp:185–216` drops old warm/observations and refreshes geometry bounds/proxy. |
| Reset/teleport | `WorldBodies.cpp:841–877` resets physical state and observation revision but does **not** erase ordinary warm rows. Fresh ordinary geometry/anchor matching is the existing warm policy. |
| Enable/disable or sensor toggle | `WorldBodies.cpp:927–976` wakes and updates observation revision, but does **not** immediately erase all ordinary warm rows. Disabled/sensor response is excluded from subsequent candidate/contact preparation. |
| Pair suppression | `WorldJoints.cpp:151–170` wakes and changes observation revision/filtering; does **not** immediately clear ordinary warm rows. |
| Mass/inertia replacement | `WorldBodies.cpp:983–998` invalidates restored continuation and wakes, but preserves ordinary guesses. Current effective mass is reconstructed next solve. This is existing behavior, not a newly proven instantaneous-reconvergence guarantee. |
| Query enablement/tags | These affect query classification, not bilateral physical response authority; they do not automatically require discarding physical support guesses. |

Ordinary warm matching does **not** use `contactObservationRevision`. That
revision guards completed physical observations/persistence. It would be false
to claim every reset, sensor change or mass update explicitly clears ordinary
solver history. The proposed correction preserves that established policy.

## Non-solver consumers and why negatives matter

`WorldSleep.cpp:39–52` follows positive warm rows during waking.
`WorldSleep.cpp:80–94,120–126` uses generation-valid positive rows to connect and
anchor sleeping islands. These consumers do not independently recheck current
geometry, enabled/sensor status or observation revisions. Therefore warm history
is not *only* a private next-step floating-point guess: it also participates in
existing support topology.

That does not establish a defect by itself. Ordinary baseline history already
spans a fixed-step interval. Relevant mutations wake bodies before their next
step, and fresh contact publication normally drops the invalid history. The
paired tests below exercise that actual boundary rather than assuming it.
Do not force sleep or clear all newly preserved guesses to hide a failing case.

## Paired source-accurate validation performed

`WarmSupportLifecycleQualification.cpp` was freshly compiled once with the
existing SupportContinuation target's Release flags/includes, using its own
ordinary main. The same new object was linked separately to:

- Frozen original archive SHA256
  `05f626d3fb5400bfd1db23ec37a3cd3e6490f8452145365662466eebccd705ec`.
- Current candidate archive SHA256
  `6b57d6c554c728a92afeb227581a863c41be9246cd2790aefe41afb3e047b4b7`.

Both executions passed **15,516 active Release checks, zero failures**. Sixteen
flat/rotated cases exercise an actually warmed box/support pair, then:

1. A real incoming sphere TOI launches the box past the finite support edge.
2. The support moves away.
3. Support collision is disabled.
4. Its bilateral mask is removed.
5. It becomes a sensor.
6. Its specific pair is suppressed.
7. The support slot is destroyed/reused with a different generation elsewhere.
8. The body is reset away from its support.

After geometric departure or removal, the actual body remains awake and
unsuspended and follows sixty normal gravity kicks with the expected
semi-implicit position/velocity (1 mm / 1 mm/s numerical check). Invalid or
departed support rows are gone after real steps. No synthetic controller status
is used in place of physical endpoints.

All 98 recorded physical endpoint/velocity/sleep/suspension/event observations
are identical between original and candidate in these fixtures. Only the two
immediate post-impact warm-load records differ, as intended: baseline zeros four
ordinary support rows; candidate retains their approximately **0.327 N·s** total,
equal to the preceding 2 kg body's ordinary `m*g*dt` estimate. It does not retain
the new impact load as ordinary support. Subsequent unsupported flight matches
and does not receive a stale support kick.

Commands, timestamps, raw logs, fixture/object/archive/binary fingerprints and
the field-by-field comparison are preserved in:

`.cache/persistent-contact-20261021/warm-lifecycle-paired/`

This is focused lifecycle evidence, not proof for arbitrary machinery, all
possible mass changes or universal save/streaming combinations. Existing M70/M77,
joint/coupling, completed-observation, save and application regressions remain
root's independent combined-candidate gates.

## Remaining focused negatives worth retaining

- Confirm elastic rebound and hard-joint/external-coupler events retain the old
  invalidation branch and actual momentum/energy behavior.
- Confirm completed contact observations continue to expose actual event loads
  independently of the retained ordinary estimates.
- Exercise a large current mass/inertia change after an inelastic event under
  the existing ordinary warm policy; classify any failure against the original
  engine before broadening this correction.
- Preserve cold-save behavior honestly: ordinary contact guesses are not newly
  serialized by this change, while completed observations retain their existing
  physical-context checks. Do not claim bit-identical arbitrary warm pile cold
  continuation merely from unchanged save code.

No additional production modification or new graph architecture is justified by
this review or the passing lifecycle negatives alone.

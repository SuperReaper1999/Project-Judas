# Deformables V2 — bounded runtime programme

Status: implementation in progress on Experimental, beginning at
`98a6114078d9f69f4f4bdea2fcef045048e1f32a`. Human acceptance remains outstanding; the six-Ural stress case is not real-time qualified.

## Approved physical contract

The ordinary vehicle preset has a permanently connected, deformable main hull.
Authorable crush and bending envelopes constrain its physical connections.
Doors, bonnet, boot, bumpers, mirrors, wheels and other designated components
may fail and detach through their actual physical attachments. Unrestricted
fragmentation remains a distinct, legitimate destructible capability.

Limits must produce physical reactions and constitutive hardening/stops, not
post-solve pose clamps, damage animations or hidden undeformed chassis bodies.
Continuity is useful cooked information for bounds, constraint preparation,
contact selection and regional activation; it does not justify disabling
folded-hull self-contact or discarding mass, inertia or transmitted loads.

## Implementation order

1. Share immutable preparation and render resources; retain independent motion,
   plastic history, ownership, overrides and failure state per instance.
2. Prove the bounded hull on a real Challenger early: frontal, offset pole and
   repeated impacts; cabin support; intact, partial and complete component
   detachment; persistent plastic deformation and cold restoration.
3. Localise material/contact work with conservative activation and same-interval
   propagation; preserve finite panel mass, reactions and full-solve fallback.
   Keep independent geometric queries separate from authoritative state changes.
4. Complete normal authoring/lifecycle integration and matched two-, four- and
   six-vehicle qualification, including repeated impacts and whole-frame costs.

## Measurement and safety

Use only corrected v2 fixtures from the architecture investigation. Its retired
v1 enclosure accidentally retained ground-sized render extents and visually
buried the vehicles; none of those measurements are valid integrated evidence.
Keep original private vehicles and current consumer SDK selections untouched.
Do not package private source images or derivatives in Git or public SDKs.

Report executable/source/input identities, rendering configuration, GPU and CPU
memory, p50/p95/p99/max, fixed-step counts, hitch sequences and all clipped or
discarded simulation time. Six-car captures with fewer than 120 frames remain
failed sample-count gates, even when useful as descriptive failure evidence.
No catch-up-cap reductions or discarded-time changes are part of this programme.
The minimum is 60 FPS in a declared supported workload, 120 FPS the target, and
5 ms combined engine critical path the stretch goal, not subsystem allowances.

A kernel-log watcher records new NVIDIA Xids during development. On any new Xid,
stop further workload launches, safely quiesce running tests and report the
error and timestamp to the operator before continuing.

## Connected hull authoring

A finite structural interface may set `JointMaterial.connected`. Its default is
false for general destructibles. Connected interfaces retain their material
stiffness, damping, yield and hardening. Cumulative plastic translation/rotation
have finite budgets; once consumed, further loading is elastic and physical
unilateral stop rows resist translation/rotation outside the authored envelope.
The solver applies reactions through ordinary MK2 Jacobians. It never teleports
sections, zeroes their velocity or releases a connection because its hull
plastic capacity was consumed. Hull connections reject explicit fracture/piece
removal requests that would disconnect the protected structure.

The editor exposes the flag per finite interface. The generic authoring command
`judas_deformable_bake connected-hull OUTPUT SOURCE [bondKey ...]` cooks selected
finite interfaces, or all finite interfaces when no keys are supplied. Select
only main-hull connections: retain ordinary finite failure laws for designated
separate components. The new asset revision `JudasDeformable7` records this
policy; ordinary fragmenting assets retain their existing representation.
Runtime plastic history remains instance-owned and uses the existing save
participant. A saved connected hull that claims broken protected interfaces is
rejected rather than silently restoring an impossible structural state.

Shared immutable geometry clones retain distinct material slots, samplers,
section ownership and per-draw palettes. Mutation of leased geometry through
vertex/topology update APIs is rejected. Destroying one instance releases only
its leases; the last instance frees the GPU geometry and image storage.

## Regional experiments and scheduling

Whole-panel and smaller-region quiet projections were tested and rejected:
insufficient material work disappeared to pay for their bookkeeping. The
independent rigid-panel proxy experiment also regressed ordinary-motion costs
because it introduced additional collision searches and mount constraints.
Their source snapshots, executables and measurements are preserved privately;
none of those physical approximation modes remains in production source.

The current candidate keeps the full material model, nodes, collision coverage
and iteration counts. Compatible physical-subdivision cohorts execute internal
constraints and self-contact as independent data batches, with rigid/terrain
contacts and attachment reactions kept on the owner thread in stable order.
Exclusive numerical state is transferred into job-owned data, helpers join, and
the owner restores every result before any shared physical phase. Failure paths
also join and restore the transferred state. Existing worker infrastructure is
reused; there is no new scheduler or competing rigid solver.

`JUDAS_MATERIAL_LANES=1|2|3|4` is the diagnostic execution policy. Lanes must not
change authoritative results. Small batches execute on the owner to avoid job
overhead. Grouping material iterations changes the former whole-panel update
ordering, so physical integration and crash fidelity require qualification;
worker completion order never determines publication or rigid reactions.

Worker kernel CPU sums overlap. They must not be added to owner-thread interval
elapsed time or presented-frame costs. Report worker item counts and batch wall
time separately. Whole-frame evidence decides whether this scheduling is kept.

Cross-panel candidate selection has a spatial-index candidate that refits both
participants after corrections and requeries later pairs in deterministic order.
It retains outward-rounded bounds and the original double-precision enclosure
check. Its net costs must also be assessed rather than assumed beneficial.


## Measurements and retained trade-offs

Ryzen 5 5600, 16 GB DDR4-2133, GTX 970 / 580.178.04, desktop NVIDIA
OpenGL 4.6. Corrected current private Ural assets, 1280×720, VSync/cap off,
shadow budget 8, ten-second warmup and twenty-second measured gameplay.
Cold means a fresh process and genuine saved pre-impact state; repeat applies
new driving impulses every 240 fixed ticks without repair, teleport or cleanup.

Same frozen executable and inputs, six Urals, full material equations and
iterations:

| Execution lanes | Profile | Mean FPS | p95 ms | p99 ms | Max ms | Lost simulation s |
|---|---|---:|---:|---:|---:|---:|
| 1 | aggregate counters | 6.90 | 175.06 | 207.01 | 238.10 | 2.315 |
| 2 | aggregate counters | 9.54 | 144.57 | 171.00 | 186.83 | 0.291 |
| 4 | aggregate counters | 21.12 | 78.32 | 139.09 | 160.34 | 0.098 |
| 1 | disabled | 7.10 | 167.54 | 202.26 | 228.08 | unavailable |
| 4 | disabled | 25.53 | 56.50 | 130.24 | 150.52 | unavailable |

Profiler-disabled captures do not expose backlog counters; absence is not zero.
Independent CPU kernel times overlap. The interval wall timer, fixed-step records
and presented-frame intervals measure different things and must not be added.
Dense depth-one captures can exceed the bounded detailed per-job scope list;
aggregate phase counters and frame/fixed-step records remain separate and intact
(178 of 256 counter slots, no event loss in those captures). An accidental depth
zero capture enabled unrestricted scope depth and overflowed: it is preserved
as rejected diagnostic evidence, not qualification.

Retained four-lane matrix, aggregate-counter captures:

| Urals | Workload | Mean FPS | p95 ms | p99 ms | Max ms | Lost simulation s |
|---|---|---:|---:|---:|---:|---:|
| 2 | cold | 162.44 | 7.87 | 8.47 | 11.23 | 0 |
| 2 | repeated | 163.39 | 9.12 | 10.27 | 34.24 | 0 |
| 4 | cold | 84.42 | 14.59 | 16.58 | 31.82 | 0 |
| 4 | repeated | 100.30 | 14.99 | 17.22 | 35.21 | 0 |
| 6 | cold | 21.12 | 78.32 | 139.09 | 160.34 | 0.098 |
| 6 | repeated | 22.06 | 118.50 | 155.38 | 173.88 | 0.298 |

Four-cold meets 60 FPS at p99 narrowly; four-repeated misses it. Two-cold's
average exceeds 120 FPS but p99 misses 8.33 ms. Six is an explicit failure, not
supported 60-FPS gameplay. None establishes the 5 ms combined-engine goal.
The catch-up cap, incoming-delta policy and simulation fidelity are unchanged;
the remaining lost time is reported as a defect of the overloaded workload.

Earlier full connected-hull serial preparation measured 9.93/10.66 FPS for six
cold/repeated Urals and 70.22/73.73 FPS for four. Phase interleaving changes
floating-point reaction ordering, so those are behavioural architecture
comparisons, not exact-state equivalence claims. Same-schedule execution lanes
are checked for exact states, including panel bending/plastic history and finite
rigid mounts. Original fragmented-hull `98a61140` evidence remains a separate
reference: changing hull failure policy is intentional and not a free
like-for-like performance optimisation.

Immutable content/image and geometry leases reduced six-Ural device VRAM from
approximately 4026 MiB in the original fixture to approximately 1.1 GiB without
reducing texture resolution. This is whole-device telemetry, including the
desktop, not exact per-process allocation attribution. Mutable physics, damage,
material overrides and section palettes remain per instance.

Raw executables, source receipts, original failures, input provenance, screenshots,
frame JSONL, GPU/OS samples and summaries are under the ignored local directory
`.cache/deformable-connected-hull-20261114/`. Private vehicle data is not shipped.
`PHASE_CANDIDATE_IDENTITY.json`, `PHASE_LANES_RESULTS.json`,
`PHASE_FINAL_RESULTS.json` and `UNPROFILED_LANES_RESULTS.json` identify captures.

## Remaining limits

- A connected hull still contains real finite-mass structural bodies; its continuity
  does not make all internal collisions or panel constraints disappear.
- Regional quiet projections and rigid proxy panels did not justify their cost and
  were removed. Full material solving is retained for active panels; ordinary sleep
  and conservative contact-candidate reuse remain effective.
- Severe six-vehicle stress is substantially faster, but still causes catch-up and
  discarded backlog. Larger pile-ups require further measured engineering before
  being declared supported real-time workloads.
- The coherent phase schedule changes numerical ordering versus the previous
  whole-panel schedule. It preserves equations, requested quality and owner-only
  rigid reactions, rather than promising old wreck hashes.
- Detachable parts must be authored with separate finite failure relationships;
  connecting every interface intentionally prevents that structure fragmenting.
- Native Windows, Android device and browser runtime performance are not established
  by the Linux desktop measurements.

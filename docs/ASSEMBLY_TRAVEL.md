# Origin shifts and physical assemblies

## One chart, one authoritative world

planetary-terrain already owned binary64 absolute terrain, a checked 512 m origin request,
a single local PhysicsWorld and a completed-fixed-boundary commit. origin-shift extends
that transaction, rather than making a second vehicle/character solver or moving
only presentation. `Simulation.cpp` commits before navigation and fixed scripts.
The existing request API remains unchanged; projects decide when to request.

For origin `O`, new origin `O'` and `delta = O' - O`:

```
absolute = O + local
local' = float(double(local) - delta)
velocity' = velocity; orientation' = orientation
```

Double-precision nodes/matrices retain double subtraction. Local offsets,
body-local contact anchors, axes, momentum, commands, scalar shaft state and
immutable assets are not positions in the simulation chart. They are not shifted.
Float rounding remains possible; this is not bit-identical global double physics.

Preflight rejects invalid anchors, legacy gameplay, far active spatial actors,
unsupported simulations and pending streaming transactions. Gravity fields are
prepared before mutation. Existing bodies, query capsules, joints and generation
handles are retained. Native owners translate their data, publish the chart and
then notify already-started healthy scripts. Callback errors fault their slot;
they cannot undo an already completed native transaction.

## Participants and ownership

| Owner | Translation / preserved state |
|---|---|
| PhysicsWorld / CharacterMotor | Existing broadphase, actual/previous body poses, world joint anchors, event points, motor/support history; retain solver body-local anchors, velocities, identities, sleep and contact filtering. |
| Wheels / powertrains | Translate current and previous contact points/visual centres. Preserve spin, suspension travel, commands, scalar rotors, gear/clutch/differential state and physical link prerequisites. |
| NavigationSystem | Translate loaded/private surface frames, obstacle world poses, links, destinations and corridor points. Detour tiles/polygon references, local obstacle data, progress, costs and revisions remain unchanged. Guidance still does not own motor motion. |
| Animation / IK / ragdolls | Translate world pose histories, reference/presentation roots, world-space targets and pending physical placements. Keep model poses, skin matrices, orientation targets and local attachment frames. Physical balance's planned footholds stay support-local. |
| Deformables / material sections | Translate actual nodes, previous/start/contact geometry and BVH bounds, root matrix and attachment target histories. Preserve strain/plastic state, velocities, reaction impulses, mass, sleep and physical part ownership. Actual rigid parts travel through PhysicsWorld. |
| Audio / particles | Translate emitter/zone/listener history and live backend voice positions without velocity reset. Shift world particles once; local particles inherit the emitter pose. Keep age, RNG, playback, group gains and explicit velocity. |
| Views / sockets / weather | Translate ordinary/render/local-player views and clip points. Socket offsets remain local; resolved poses inherit translated owners. Weather/planet sources are already absolute. |

These are positional changes inside existing owners, not a generalized registry
that exposes internal pointers to scripts. Inactive games do not run a new
per-frame participant scan; the extra work is requested origin publication.

## Streaming, saves and lifecycle

Snapshot records retain their capture origin. Previously wheels, mechanical and
other opaque component records prohibited origin changes while suspended. origin-shift
removes that blanket rejection and performs explicit reconstruction:

- Kinematic target points shift **before** the existing motion validation. Staged
  publication replays re-encoded current-chart commands, not old-chart bytes.
- Deformed nodes and their initial frame shift immediately after native decoding.
  Owned coarse material sections and world-fixed support anchors shift before
  creation; normal generation-safe remapping still resolves attached bodies.
- Authored world-space IK intent translates with restored definitions. Scalar
  wheel/powertrain continuation stays unchanged; derived contact data rebuilds.
- Retained bytes and their capture origins are never rewritten in place. The
  existing versioned archive layouts remain unchanged. Full saves preserve the
  current chart; a normal scene reload restores the authored chart.

Live articulation suspension restrictions remain intact. Such an assembly can
travel through origin changes, but still pins its source region until its usual
retirement/adoption rules permit unloading. Pending region installation/retirement
continues to block publication rather than allowing a partial rebase.

## Script caches

[The optional `onOriginShift(event)` hook](judasjs/planetary-terrain.md#onoriginshiftevent--explicit-script-caches)
lets projects translate known local caches after publication. Arbitrary script
JSON is never inspected or changed heuristically. Keep long-lived locations in
absolute coordinates, especially when a script can be suspended; absent scripts
receive no missed notifications. Rebuild local caches when the instance resumes.
The hook is synchronous/budgeted/error-isolated, and fixed-only writes remain
restricted to `fixedUpdate`.

## Boundaries / remaining work

- One shared local chart, checked at 8 km; absolute anchors are finite and bounded
  to 1e12 m. This does not enable simultaneous widely separated active physics.
- Grid Water regions translate through their existing owner. Analytic atmosphere
  frames and combustion carrier/scalar state have origin handling; that does not
  imply save/suspension participants. Retired PBF/reservoir owners are absent.
- Planet collision retains one bounded focus; the lab prepares ahead and brakes
  if collision is not ready. It does not silently drive through missing terrain.
- Pending streaming blocks a shift; large travel still requires residency policy.
  There is no transparent suspension of arbitrary live constraints/articulations.
- Floating-point solvers can diverge slightly after translation. Reported matched
  errors and tolerances are evidence for the exercised fixtures, not a global
  determinism guarantee. Continuous active travel was measured over 2 km; the
  authored watch route contains waystations over approximately 11 km.
- Windows/Android origin-shift builds and physical-controller validation are unperformed;
  they are informational/nonblocking under the established workflow.

# `liquid` and `Entity.liquid` — conserved liquid (M54–M55)

Judas owns quantities, geometry and transfers. JS owns why a game drains, scoops
or displays water. This API requires **Project Settings → Experimental → Advanced
Fluid Dynamics**, disabled by default. It is separate from basic-water swimming
through `world.fluidSample` (which can also sample an opted-in legacy PBF field).
See [project policy and basic water](../PROJECT_SETTINGS_AND_WATER.md). When advanced
fluids are disabled, authored owners remain inert and `entity.liquid` is null.
Units: m³, kg/m³, metres, seconds, N·s for impulses. One litre = .001 m³.

## `entity.liquid: LiquidVolume | null`

Available after its CPU assets load and a fixed step registers the component.
Query again until non-null. `LiquidVolume.handle` is an opaque world-generation
token: do not construct/parse it. `valid` becomes false after removal/world
replacement; state/control on stale handles throws ReferenceError. See
[entity lifetimes](entities.md) and [scene/session lifetime](scenes-state.md).

## `LiquidVolume.state`

Detached snapshot: `entityId`, `entity`, `enabled`, `equilibriumValid`, `container`,
`material`, `density`, `volume`, `capacity`, `stableCapacity`, `coordinate`,
`surface`. `coordinate` is the basin-local equilibrium distance/radius; for a
moving surface it is only the equilibrium reference, not every local height.
`surface` is null without dynamic capability; otherwise its snapshot contains
`enabled`, `cells`, `faces`, `volume`, `iterations`, `retries`, `limitedFaces`,
`residual`, `partitionError`, `stepSeconds`. Surface volume partitions the owner's
volume and is never counted again in accounting. Residual is equation error in
m³ before flux limiting, separate from ledger tolerance; the solve target is the
authored tolerance times `max(1, owner volume in m³)`. `capacity` is available physical storage.

## `LiquidVolume.enabled = boolean`

Disable query/flow participation while retaining owned volume. Reload restores
authored initial volumes. Destruction retains water in a parked parcel.

## `LiquidVolume.surfaceEnabled = boolean`

Pause/resume surface dynamics, retaining allocations and flow. Requires a dynamic
basin. This does not disable ordinary ownership/transfer or moving-solid storage
updates. Resuming uses retained state; it does not reset waves. Fixed-update only.

## `source.transferTo(destination, cubicMetres, options = {}): number`

Fixed-update only. Returns the actual transfer bounded by availability/capacity.
Options: `sourcePoint?: Vec3`, `destinationPoint?: Vec3`, in local simulation
world coordinates. Spatial calls start at that cell and traverse its wetted face
component; amount-only calls use a deterministic wet component and proportional
allocation. Neither flattens waves or crosses closed/dry connections. A completely
dry dynamic receiver may initially accept only one cell's capacity.
Withdrawal carries away the donor fraction of existing flow momentum. Refill
starts incoming quantity at rest while retaining existing wave/flow state;
explicit parcel arrival impulses supply momentum separately.

Same material identity/density required. Negative/nonfinite requests throw
TypeError; stale handles throw ReferenceError. Disabled/self/incompatible transfers
return zero. Debit/credit happen once. This is plumbing/storage control; physical
scooping/pouring use authored openings automatically.

## `LiquidVolume.applyImpulse(point, impulse): boolean`

Fixed-update only. Adds tangential momentum to adjacent face discharges of the
selected dynamic cell. Point and impulse use simulation-world coordinates. No
water is created; the gravity-normal component is discarded. Returns false for
inactive/non-dynamic surfaces; stale handles throw. This is a depth-reduced
impulse, not an exact 3D fluid/body momentum solver.

## `liquid.sample(point): LiquidSample | null`

Authoritative occupied-water sample at a point in the current world chart.
Null when dry/disabled/outside available geometry (including excluded solid space).
Fields: `entityId`, `entity`, `material`, `density`, `depth`, `coordinate`,
`surfacePoint`, `normal`, `up`, `velocity`. `up` is local anti-gravity;
`normal` is the represented surface normal. Do not interchange them.
`velocity` is modeled tangential surface flow; static M54 bulk flow is zero.
Finite-volume levels are piecewise constant; radial tetrahedra approximate curved
coordinates within the declared bake bound. There is no universal world-Y.

The binding converts the input to binary32 coordinates. At a cell boundary,
rounding during an origin shift can select a different occupied cap even though
the liquid state is unchanged. Distinct pre-shift points can become the same
post-shift input; this API cannot recover their original identities. Do not assume
exact boundary-point sampling invariance through an origin change. See the
[measured query-identity limitation](../evidence/prealpha-pass5/liquid/REPORT.md).

## `liquid.samplePresented(point): LiquidSample | null`

Same shape, sampled using previous/current local surface coordinates and the
current presentation alpha in `presentationUpdate`. Elsewhere alpha is 1.
Use with `entity.presentedTransform` for presentation; physics/swimming use
`sample` in fixed updates. Rendering uses this same interpolation state.

## `liquid.submerged(entity)`

Actual enabled box/compound collider integration. Returns `volume`, `center`,
`buoyancy` (newtons). Missing/stale collider throws ReferenceError; unsupported
shape throws TypeError. Read-only; automatic forces require LiquidInteraction.
Dynamic loading samples occupied cell envelopes; one hydrostatic/drag path applies
forces through normal PhysicsWorld.

## `liquid.accounting(material = 'water')`

Snapshot m³ totals: `reservoirs`, `containers`, `detached`, `total`, `expected`,
`error`, `tolerance`. Expected includes authored/runtime-spawned initial water.
Materials have separate ledgers. Geometry/solve errors never edit the expected
total. This is a live runtime snapshot, not a save-state migration operation.
[M61 slots](saves.md) persist the quantity ledger and dynamic surface partitions/
flows together; neither queries nor restoration invent a second owner of water.
Legacy deltas omit this state, and ordinary scene reload restores authored amounts.

## Diagnostics

`liquid.errors`: `{entityId,message}[]`. `liquid.connections`:
`{entityId,active}[]`, above the authored saddle. Deterministic entity order.
Failed bounded surface solves roll back the attempted dynamic step, retain water
and report an error; they do not switch to decorative motion. Independent accepted
external transactions remain accepted. See [surface method/authoring](../LIQUID_SURFACES.md).

## Boundaries

Physical disjoint tetrahedral cavities; static equilibrium or optional bounded CPU
finite-volume surfaces. Uniform and constant-magnitude radial conservative gravity.
No dynamic deep grid/particles, overturning waves, sealed-air physics, live rebasing
or nonconservative equilibrium. Small containers retain their local quasi-static,
vented lip model. Parcels are conserved ballistic quantities without a droplet
pressure solver; unsupported landings park water. Dry body mass/inertia are not
rewritten from contained quantity. See [M54 authoring](../LIQUID_RESERVOIRS.md),
[physics](physics.md), [character](character.md) and the
[copyable surface example](examples/liquid-surface.js).

Unsupported/zero-gravity containers retain volume and enabled state but set
`equilibriumValid=false`; queries, surface and transfers suspend. Returning to
supported gravity resolves retained water at the current pose and clears the
error. Coordinate/stableCapacity remain last-valid values while suspended.

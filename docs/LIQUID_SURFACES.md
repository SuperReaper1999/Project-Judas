# Dynamic liquid surfaces — M55

**Status through M69:** M55 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Starting checkpoint: `d9ebc8c7a987047b1d4175ed5d5de7da8dea472d` (accepted M54).
Current candidate; operator visual/interactive acceptance is pending.

M54 owns liquid quantity. M55 optionally partitions a reservoir into modest
surface cells. Deep water remains implicit; there are no dynamic depth layers or
bulk particles. JavaScript owns game controls and swimming. Legacy PBF and
protected P1 research remain separate, unchanged systems.

## Author and bake

Open `projects/liquid_surface_demo/liquid_surface_demo.judasproj` in the editor.
A Liquid Basin uses the existing physical `.judascavity` asset. Enable **Dynamic
surface**, choose 2–32 columns/rows (at most 1024 cells), friction, pressure budget,
solve tolerance and splash settings; **Bake liquid capacity**, then save the scene.
The CLI equivalent is `judas_liquid_bake PROJECT SCENE_RELATIVE_PATH`.

Non-opted basins retain M54's `JudasBasin1` format and source fingerprint. Opted
basins write `JudasBasin2` with a bounded `Surface1` section. Source/settings changes
require rebaking; normal resource demand, prefab serialization and export use
stable asset identity and reject stale bakes. No scene/save fingerprint schema
bump is needed: serialized authored settings already contribute to schema 5.
Runtime liquid state is not a new save-state format.

The bake partitions actual tetrahedral cavity space. Plane cells are clipped
columns; central-radial cells are angular cones in a gnomonic cap chart, with
geodesic centre distances and actual shared face apertures. The supported cap is
smaller than a hemisphere. A globally convex cavity certifies convex cell
intersections; other authored cells are validated individually. Disconnected or
nonconvex coarse subregions reject rather than silently becoming one pool.
Topology uses original authored geometry; radial quadrature uses the existing M54
refiner within each cell and retains the declared coordinate-error bound.

## State, method and boundaries

`owner.volume = sum(cell.volume)`. Cells are that same owned water, never extra
ledger entries. Double quantities flow through LiquidSystem transactions only.
One signed face discharge debits/credits the same accepted volume:

```
Qnew = exp(-friction * dt) Qold - dt * g * wetArea / distance * (qb - qa)
C_i(qnew) - V_old + dt * orientedSum(Qnew) = 0
```

Local physical storage C(q) is piecewise cubic in the affine tetrahedral coordinate.
A positive simplex-spline recurrence and a local-coordinate interval tree cache
its coefficients without global derivative cancellation. Inversion is bounded
bisection. Solid unions are subtracted before re-tessellation; unaffected cells
retain cached geometry/storage. Convex fragments use an existing-vertex boundary fan, skipping incident faces, to avoid redundant centroid subdivisions. No per-frame static bake or dense repeated
capacity sampling is required. Changed solids stage only affected face apertures;
exact head/area results are memoized until their geometry changes. Cap extraction
reuses its edge topology within each vertex-coordinate interval, recomputing the
actual intersections at the presented level. These caches change work performed,
not physical geometry, cadence or accepted quantities.

The semi-implicit nonlinear pressure/continuity solve uses bounded Newton/PCG
iterations and line search, frozen wet apertures and real depth. Dry/full-cell
Jacobian regularization does not add water/storage. Exact empty/full boundaries
use the physical interior one-sided storage derivative; only outside storage is
the derivative regularized. The maximum cell equation
residual target is `settings.tolerance * max(1, ownerVolume)` in m³. Reported
pressure residual precedes any shared flux limiting; limiter activity and final
partition error are separate diagnostics. Shared donor/receiver flux
bounds and remaining per-face budgets preserve positivity before commit, including
nearly empty donors subject to floating-point rounding. There is no post-update volume clamp,
mean-height repair, total rescale or fictitious shallow floor. Bounded half-step
retries roll back all attempted dynamic state if either half fails; diagnostics
retain the failure. Accepted earlier external transactions remain accepted.

This is a first-order local-inertia/hydrostatic model, with exponential linear
friction and no nonlinear momentum advection. It approximates long waves; it is
not accurate spectral deep-water dispersion, full CFD, overturning sheets or
violent container slosh. Face geodesic distance is frozen at the baked rim metric;
radial storage/surfaces use M54's bounded affine-coordinate approximation.
Geometric approximation, equation residual and quantity ledger tolerance are
separate. See [development discretization](evidence/m55/development/CORE_METHOD.md).

## Transfers, bodies and cups

Spatial transfer options start from the chosen world point's cell and traverse
its wetted connected component. Amount-only calls select a deterministic wet
component and distribute proportionally. Dry/closed barriers are not bypassed;
a dry dynamic receiver may initially accept only one cell's capacity. Withdrawal
removes the donor fraction of existing discharge with its water; unaffected faces
and retained wave state remain intact. Amount-only refill starts incoming quantity
at rest and preserves existing discharge; explicit arrival impulses add momentum
separately. Re-wetting proceeds through physical open faces without resetting the
whole surface. See the [drain/refill correction and proof](evidence/m55/refill-followup/README.md).
Spill graph connections use local head at the
authored opening (nearest boundary cell when the saddle is between reservoirs),
remain one M54 operation, and obey their authored saddle/enabled state.

Enabled box/compound colliders remove actual available space, including overlaps
as a union. A hollow cup is its compound walls plus the actual owned wet cavity,
not its exterior bounding hull. Cup contents are debited from surrounding water;
a filled cavity is excluded from reservoir occupancy. Opening sampling uses the
open polygon interior, because rim vertices are solid boundaries. Cups retain
M54's quasi-static, freely vented, orientation-aware cavity/lip model.

Displaced water moves along wetted face paths. If no accessible room exists,
conserved overflow is explicitly parked as parcels. Pouring debits the cup into
ballistic parcels; actual arrival credits the receiver cell. Incoming tangential
momentum disturbs receiver flow. Strong local modeled flow may eject a small real
quantity with velocity whose kinetic energy is debited from a stated 10% local
face-energy budget. A parcel budget suppresses ejection; it never deletes water.
Unsupported dry-land arrivals remain parked, as in M54.

Fixed-step order: retain previous surface -> JS fixed controls -> register/load
owners, cup opening exchanges, graph transfers, parcel motion/reception -> update
solid storage/apertures -> surface solve/splash -> one hydrostatic/drag loading
path -> ordinary rigid integration. Gravity remains Judas's selected equilibrium
field. Buoyancy integrates occupied cell envelopes before the tested body's own
excluded space; immutable envelope bounds/plane equations are cached and drag uses modeled flow. This is approximate coupling, not exact
combined body/liquid momentum or energy conservation. Dry body mass/inertia are
not automatically rewritten from contained water.

## Query, render and swim

[Liquid API](judasjs/liquid.md) and [executed example](judasjs/examples/liquid-surface.js):
`liquid.sample`, `samplePresented`, `submerged`, `accounting`,
`LiquidVolume.transferTo`, `applyImpulse`, `surfaceEnabled` and surface diagnostics.
Authoritative samples report actual available occupancy, level/depth, gravity `up`,
represented surface `normal`, surface point and modeled tangent velocity.
Gravity-up and surface/support normals are separate. Coordinates are local world
coordinates around the fixed double absolute origin; no live rebasing/world-Y.

Presentation samples and mesh use the same previous/current levels and alpha as
presentation-facing cameras. Finite-volume caps are piecewise constant, with
actual exposed shared-face bands closing height steps. They are not a decorative
smooth-wave shader. Renderer owns GL resources, transparency, per-camera layer
mask/frustum submission and water-path texture; offscreen liquid still updates.

Underwater attenuation uses a 32×18 ray grid per camera, intersecting actual
occupied available geometry at presentation alpha. The shader clips water length
to each world fragment and the first connected wet interval. Sky and HUD do not
share a camera-centre tint flag. This is coarse nearest-sampled optical path
length, not refraction/reflections/caustics; particles and later separated water
intervals are not volumetrically attenuated. Secondary cameras calculate their
own paths without another simulation tick.

The demo controller's JS reads authoritative liquid samples and supplies generic
acceleration/velocity to CharacterMotor. It composes gravity opposition, flow-relative
drag and user movement; there is no native swimming mode or special demo branch.

## Demonstration and controls

- `Scenes/lab.judas`: 16×12 m sloping pool, 160 m³ initial water, 24×16 surface
  grid; smaller tapered vessel with 1000→200→1000 L Q/E experiment; connected
  vessels, floating bodies, ordinary bucket prefab, receiving/storage basins.
- `Scenes/radial.judas`: actual radius-20 planet, 8×6 m terrace lake with 65 m³,
  16×12 surface grid, radial storage/free surface and tangential flow, access
  steps, ordinary bucket and buoyant body. The basin has an authored terrace
  floor; the free surface uses radial coordinates, not rotated uniform physics.

WASD / controller: move; mouse / right stick: look. Space launches or swims up;
C swims down. G picks up/drops a tagged body, T throws, P held tilts the held
bucket, wheel adjusts carry height. V adds a real tangent momentum disturbance
where the view enters water. Q/E transfer 800 L out/back; Z/X drain/refill the
large pool (20 m³ flat, 10 m³ radial). F/H fill/drain spill vessels in flat mode.
1/2 switch registered scenes; R reloads; Esc pauses with authored runtime UI.
HUD reports owner/container/detached totals, accounting error, cells and solve cost.

## Human review

1. Disturb the small vessel and pool; watch propagation, reflection and settling.
2. Q: 1000→200 L; E: restore 1000 L without flattening motion. Check totals.
3. Enter/swim/exit the sloping pool; cross the waterline and inspect underwater view.
4. Throw/move a body through water; check displacement and sensible buoyancy.
5. G: grasp bucket; dip its opening, carry real water away, P: pour into receiver.
6. Drain/refill the large pool and check exposed shoreline/retained waves.
7. Switch to radial lake; repeat waves, body/cup use and local-gravity swimming.
8. Confirm responsiveness throughout, including moving buckets and underwater view.
9. Pause/resume and reload/Stop; confirm no stale water/handles.
10. Run the moved exported package; confirm equivalent behaviour.

Final isolated desktop-loop medians were 17.11 ms/frame (flat) and 18.56 ms/frame
(radial), at 60 Hz authoritative stepping. The radial moving-solid/cup case remains
more expensive: 13.36 ms median fixed step, 15.80 ms p95. These short automated
probes do not certify extended interactive performance; see the complete phase,
frame-tail and catch-up measurements in the evidence.

Measured results, preserved failures, source fingerprints and exact changed-file
inventory belong in [M55 evidence](evidence/m55/README.md). Automated geometry and
screenshots are not human interactive/visual acceptance.

## Current approximation limits

- Hydrostatic local-inertia model without nonlinear momentum advection, breaking
  waves or spectral dispersion. Piecewise-constant caps deliberately show the
  finite-volume discretization. Radial storage retains the declared affine
  curvature bound; face geodesic metrics are frozen at bake time.
- Owners use the existing M54 unit entity-scale restriction. Box/compound solids only. Authored disconnected/nonconvex coarse cells reject;
  subcell topology changes created by moving solids are not a resolved flooding
  model. Moderate body/cup motion is the supported interaction scale.
- Presented levels interpolate previous/current states. Solid exclusion geometry
  is the latest authoritative fixed-step geometry, not a separate interpolated
  geometric re-bake. Container interiors remain M54 quasi-static, with its
  documented mass/inertia and unsupported landing limitations.
- No liquid dynamic-state save migration, arbitrary changing equilibrium fields,
  zero-gravity blobs, whole-planet streaming or general dry-ground flooding.

Optical acceleration caches physical tetrahedron planes and a local bounded BVH plus a small per-camera cell-bounds tree.
Changing solid geometry invalidates affected cells; each camera still intersects
its own rays against the current presented level. Offscreen physics is unaffected.

Splash emission reserves both parcel kinetic energy and a conservative lifting
allowance using the full represented column depth, emergence distance and declared
radial coordinate error. This may suppress small splashes; it does not manufacture
liquid or claim exact combined fluid/body energy conservation.

# Conserved liquid reservoirs — M54

Accepted M54 foundation, originally based on `16f7d59fa3289e7e7c43aaeb7e88471be6fe348f`.
It is separate from retained legacy PBF and protected P1 research. M54 itself
had no waves; [M55](LIQUID_SURFACES.md) adds optional dynamic surfaces while
M54 quantity ownership remains authoritative.

## Ownership and units

RuntimeWorld owns LiquidSystem. Each volume belongs to exactly one reservoir,
container or detached parcel. Transfers debit/credit together, bounded by source
availability and destination capacity. SI: m³, kg/m³, metres, seconds. Mass is
volume × density. JS and presentation never own another copy of the quantity.

`liquid.accounting(material)` reports reservoir/container/detached/expected/total
and error. Expected volume includes explicitly authored or spawned initial
quantities. Deterministic entity/parcel ordering; double arithmetic. Ledger
acceptance is `max(1e-12,64*epsilon*max(1,expected)*(owners+parcels+1))` m³.
This floating-point tolerance is independent of geometric bake error. No ledger
repair deletes or invents volume to match geometry.

Disable retains quantity but excludes queries and transfers. Destroy releases
owned volume into a **parked conserved parcel**. Reload/scene replacement/Stop
ends that runtime session and reconstructs authored state. Tokens contain a
world generation and monotonically allocated owner ID. They cannot alias a new
owner or another world.

## Physical geometry and capacity

`.judascavity` is a disjoint union of closed positive-volume tetrahedra in local
metres (`JudasCavity1`, count, four xyz vertices per cell). This is authored physical
geometry, independently of render meshes. Degenerate, overlapping or unbounded
input is rejected. M54 has static basins and modest rigid containers; unit entity
scale is required. A connected/disconnected union of tets can express irregular
floors, tapered sides and multiple depressions.

The bake clips each tetrahedron, integrates volume/first moment from its actual
faces, and builds monotonic `(q,V)` data in `.judasbasin`. Every vertex-coordinate
knot is retained. Between knots capacity is cubic; quarter/mid/three-quarter
linear-interpolation residuals ×4 bound that cubic's error. Adaptive subdivision
honours authored volume/height tolerances. Linear tank intervals need only two
samples even when very deep. Inversion is binary search + interpolation, O(log N).
Bounds and equilibrium surface are cached; presentation is downstream.

The `.judasbasin` header stores source/settings fingerprint, supported equilibrium
class, gravity data, tolerances, curved-coordinate bound and full capacity.
Canonical authored fingerprint schema remains **5**. Conditional M54 component
fields and contributing source/baked asset bytes enter the effective world
fingerprint. Absolute project/export paths do not. Legacy scenes have unchanged
canonical fingerprints. M54 quantities are session state; the existing world-save
format does not serialize this new ledger. Do not use saved deltas to resume a
liquid transfer experiment; reload restores authored amounts.

## Gravity and precision

`GravityField::Equilibrium(position, descriptor)` is an optional conservative
capability, forwarded by the same GravityContextMap selector as acceleration.
Unsupported/zero-gravity fields fail honestly. Liquid consumers do not type-check
FaithfulGravity/RadicalGravity.

Uniform gravity uses q = dot(actual anti-gravity, local position); potential is
`g*q`. Judas's existing constant-magnitude radial gravity uses q = radius and
potential `g*r`. Its free surface is curved, not a world plane or inverse-square
assumption. Radial tetrahedra subdivide using a Hessian/diameter bound on radius
interpolation. Rendered/query geometry approximates that curved equipotential
within the header's coordinate bound; capacity-table tolerance applies to this
refined physical representation. It is **not** a claim of that same tiny volume
error against an exact spherical integral. Independent spherical cross-section
checks exercise the curved geometry bound.

A container entering unsupported gravity retains its quantity, suspends queries/
surface/flow, and reports `equilibriumValid=false`. Returning to supported gravity
resolves the retained volume at its current pose and clears the diagnostic.

Small vented containers sample gravity at their current position, use local
uniform clipping and solve a plane offset anew when rotated. Large cavities with
materially varying gravity and trapped/compressed air are unsupported. Basin
bakes require one consistent equilibrium field across sampled source geometry;
a basin spanning gravity zones must be divided by the author. The scene's fixed
double absolute origin is separate from local float physics positions. The demo
uses `(1e12,-2e12,3e12)`; there is no live origin rebasing.

## Connections, openings, transport

A LiquidConnection references two basin entity IDs. Its entity position is the
saddle in the applicable equilibrium coordinate. Below the saddle reservoirs
retain separate quantities. Above it, bounded Torricelli-style flow redistributes
without duplicating owners. Equalisation limits prevent timestep overshoot.
Closing a connection preserves each residual pool. This is an authored spill
graph, not automatic watershed discovery or channel CFD.

A LiquidContainer has an explicit planar exterior opening polygon, cavity,
material and flow area/coefficient. No inferred “top” or fill-percent state.
When its opening contacts compatible bulk water, external head establishes a
target cavity volume. Only the bounded deficit transfers. Removing the opening
retains acquired water. Rotation recomputes the lowest lip and stable clipped
capacity; excess emits gradually with bounded hydraulic discharge.

Each emitted ballistic parcel immediately owns its debited amount. It follows
actual Judas gravity. Existing M44 solid queries bound its swept path before
receiver tests. Containers accept through an explicit opening; basins accept
within physical cavity geometry. Coarse bounded segment sampling is not a full
liquid CCD solver. The source collider is ignored as the stream leaves its lip.
Unsupported solid landings retain parked visible parcels; they do not evaporate
or spread across arbitrary dry ground. Expected demo pours have a receiving basin.
No droplet pressure, PBF/SPH, waves or spray solver.

## Queries and bodies

[Liquid JS reference](judasjs/liquid.md): `Entity.liquid`, safe LiquidVolume,
`liquid.sample`, `liquid.submerged`, diagnostics and fixed-step paired transfer.
Point queries require actual cavity membership below the solved coordinate.
Depth, density, surface point and anti-gravity normal are snapshots. Baseline bulk
velocity is zero. Box/compound submerged volume and centroid use real collider
tetrahedra intersected with the occupied region.

Opt-in LiquidInteraction applies density × displaced volume × gravity, at the
displaced centroid, with bounded drag through ordinary PhysicsWorld forces.
No particle fill is required. M54 does not yet subtract moving solid displacement
from the static capacity solve or raise the free surface around an entering body.
The submersion field is the equilibrium background region; hydrostatics do not
claim a dynamically displaced surface. Dry body mass/inertia are not rewritten by container
water in M54; no exact CFD momentum-conservation claim. Compound overlapping
collider boxes can overestimate displaced volume; author disjoint geometry.
An explicitly M54-owned body is excluded from legacy PBF loading. A basin/cavity
must not also be registered as the same particle body of water. Separate authored
liquid representations remain distinct. Legacy non-M54 PBF behaviour is unchanged.

## Editor / resources / prefabs / export

Add Liquid basin/container/connection/interaction in the ordinary inspector.
Select a tracked physical `.judascavity`; set material, density and initial m³.
Bake capacity, inspect table/capacity/tolerances, preview geometry, or clear/rebake.
Stale content/settings/gravity reject startup/export. The CPU-only baked/cavity
assets use normal asynchronous ResourceManager demand/lifetime/eviction.
No startup rebake. The command-line companion uses the same bake implementation:

```sh
build/judas_liquid_bake projects/liquid_reservoir_demo/liquid_reservoir_demo.judasproj Scenes/lab.judas
build/judas_liquid_bake projects/liquid_reservoir_demo/liquid_reservoir_demo.judasproj Scenes/radial.judas
```

Normal component properties round-trip, participate in generic prefab overrides,
and spawn through SpawnPrefab. Connection references remap with prefab IDs.
M54 also fixes the shared prefab property path to preserve repeated compound-box
and cavity entries instead of overwriting all but one entry.
Export packages both registered scenes and their normal liquid/script/UI/font
assets. Moved packages use the ordinary project-root resolution.

## Demo and controls

Project: `projects/liquid_reservoir_demo/liquid_reservoir_demo.judasproj`.
`Scenes/lab.judas`: tapered irregular MAIN (1,000 L); storage; receiving tank;
separate graph-water double-depression experiment (1,800 L); physical 25.088 L
bucket prefab and buoyant box. Main is left, receiver right, spill experiment at
far right. Water is an equilibrium surface, not particles throughout its depth.
`Scenes/radial.judas`: real oblique radial lake (2,000 L), storage, buoyant box.

| Control | Meaning (project JS) |
|---|---|
| WASD / left stick; mouse / right stick | Move/look |
| Space | Scripted launch |
| Q / E | Transfer exactly 800 L to storage / return |
| F / H | Fill spill experiment / drain below saddle |
| G | Ray-select pickup or drop held body |
| Mouse wheel | Lower/raise carry point |
| Hold P | Physically torque held bucket sideways to pour |
| T | Impulse-throw held body |
| N | Spawn another empty bucket prefab |
| 1 / 2 | Registered lab / radial scene |
| R / Escape | Authored reload / pause |

Hold the bucket, look down into MAIN and lower it until its opening is submerged.
Lift it out; carry to the receiving tank; lower it over the cavity and hold P.
For a clean 1,000→200→1,000 L proof reload first, without bucket extraction.
After scooping the main ledger legitimately contains less water.

## Human review

1. Lab: Q shows 200 L and lowers the surface; E restores 1,000 L and the surface.
2. F connects two depressions; H disconnects them with residual pools preserved.
3. G/wheel dips the bucket. Main loss equals bucket gain; carry retains volume.
4. Over receiver, P pours through air. Observe bucket/detached/receiver and TOTAL.
5. Drop the buoyant body into water. No deep particles do the work.
6. Scene 2: drain/refill curved water; inspect oblique gravity and buoyant body.
7. Reload/Stop removes runtime containers/parcels and restores authored quantities.
8. Run the moved exported package; repeat the same operations.

Human visual/interactive acceptance is authoritative. See
[M54 evidence](evidence/m54/README.md) for measured tests, costs and limitations.

## M55 optional extension

The static M54 foundation remains available. See [dynamic liquid surfaces](LIQUID_SURFACES.md) for optional conserved finite-volume partitions, local transfers and current surface/swimming authoring.

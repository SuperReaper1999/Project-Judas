# Structural fracture and physical fragments

## Ownership and reused facilities

deformable shipped shared immutable `.judasdeform` topology, per-instance double local
node state, CPU XPBD cloth/tetrahedral solids, bounded plastic rest flow, surface
contacts/attachments, normal async resources, mapped GPU presentation and save-slot
participation. Its limits remain: no general shape-query/CharacterMotor collision against
soft surfaces, no cloth tearing, no arbitrary remeshing, bounded deformation.

fracture extends that same asset/component/ownership path. A version-2 cook adds
physical cells and explicit interfaces; version-1 deformable cooks remain unchanged.
RuntimeWorld owns independent irreversible state. deformable solves soft cells; joint fixed
constraints and the ordinary PhysicsWorld solve rigid cells. No new library or
second solver. Renderer remains the only raw-GL owner. Liquid solvers are unchanged.

Each cell owns its nodes/tetrahedra exactly once. Coincident nodes across interfaces
are deliberately duplicated, with mass lumped from their incident tetrahedra rather
than copying whole-parent mass. Stable part/interface keys are unrelated to names,
render UV seams or generation-aware body slots. Broken bonds stop carrying load;
a graph derives connected components in stable minimum-part order. Alternate paths
remain connected. There is no health counter or scripted neighbour cascade.

## Physical demand and limits

- Soft interfaces use vector XPBD cohesive constraints. The final accumulated
  multiplier divided by **substep seconds squared**, with the constraint gradient,
  supplies traction; iteration corrections are not charged as separate impacts.
  Resultant and peak nodal traction distinguish tensile opening from tangential
  shear. A polar estimate rotates the interface normal with its current material.
- Rigid interfaces observe the signed accumulated joint solved reaction impulse over
  the authoritative step. Their box-face bending contribution uses moment divided
  by an approximate section modulus. This is a conservative game approximation,
  particularly at contacts/finite solver iterations, not measured concrete/steel.
- Tensile/shear strengths are Pa; cohesive compliance is m/Pa. Elastic compliance,
  material density and plastic yield remain separate. Default compression does not
  crush; an optional positive compression threshold enables brittle crushing.
  No fatigue, healing, damage-per-frame or arbitrary crack propagation.
- The core specimen survives 5 N and fails at 400 N at 4×4 and 8×6 quality. Force
  estimates differ materially between those settings; evidence reports that
  sensitivity rather than claiming invariance or certified stress accuracy.

## Fixed boundary and physical pieces

Failure candidates are recorded during the normal step. Topology commits after
that step, before `onFracture` callbacks. Explicit cuts are separately identified.
The finite cook preallocates its material partition; at most 256 parts, 1024 bonds
and 16 node pairs/interface. Invalid partitions or insufficient piece budgets fail
without publishing a partial topology. This is a capacity bound, not a hard deadline.

Soft pieces retain current nodes, velocities and plastic rest state. Their original
deformable material continues deforming after separation. Rigid cooks publish **all finite
mass cell bodies and fixed bonds atomically at initial readiness**. Breaking later
removes bonds; it does not replace an intact parent with pristine copies. Connected
rigid cells stay a normal fixed-joint assembly, not a newly merged compound island.
The logical family remains nonphysical and renders their common mapped topology.
It owns neither a duplicate parent collider nor duplicate mass. Children do not
receive copied scripts/components. Intact adjacent pair collisions are suppressed;
failed neighbours immediately use ordinary collision filtering again.

Initial rigid fitting preserves nodal mass/COM and linear/angular momentum using
that same lumped inertia. Each body receives point velocity and angular velocity;
no triggering impulse is replicated. Proxy volume is not the material mass model.
One oriented box per cell must cover its material and not overlap another cell's
interior. The imported L assembly is three such cells. General concave single-cell
collision, custom render bindings and conversion of a deforming soft instance to a
rigid one are unsupported. Nonuniform cell-density weights are rejected.

Supports use existing named deformable groups in world or body-local frames. Rigid world
supports are ordinary fixed joints; dynamic supports get normal joint reaction.
Rigid bone supports/runtime support additions are rejected. An ordinary external
joint joint anchored on a surviving cell survives other material removal. A missing
support releases safely; it cannot alias a reused body generation.

## Rendering and collision support

Cooked oriented boundary corners retain outer UVs; hidden internal faces become
visible in material slot 1 after a cut. Normals/tangents, previous/current poses,
shadows, camera-target cameras, render layers and conservative bounds use normal resources.
Render passes do not step fracture. Geometry buffers rebuild only on topology
revision, while normal vertex updates use the deformable presentation path.

| Consumer | Rigid cells | Soft fragments |
|---|---|---|
| Ordinary rigid contact, gravity, impulses | PhysicsWorld boxes/compound primitives | Existing sampled deformable surface contact/reaction |
| Other soft surfaces | Existing deformable-to-rigid contacts | Existing deformable surface/self-contact, separate components |
| shape-query ray/shape casts; CharacterMotor | Normal public physics geometry | General blocking/casts unsupported; `deformable.raycast` available |
| collision-event events | Normal body semantics, other handle is physical child | Soft-surface events unsupported |
| Audio obstruction | Normal rigid query path | No new soft occlusion path |

No stale family AABB is a narrowphase collider. The game fixture proves a real
opening, audio obstruction clearing and public JS navigation/motor passage without
explicitly deleting the pieces. Moving debris can temporarily obstruct that hole.
Its walkable floor is independently baked; JS disables one ordinary navigation
obstacle only after both clearance rays pass. Existing agent repath cadence applies
(0.5 s here); this does not rebake fractured floors or promise debris-aware navmesh.

## Authoring and current content

The Deformable inspector can create cohesive/rigid block/beam/panel grids, select
existing support groups, edit cooked strengths/compliance, preview part proxies,
interface normals/interior edges, or import an assets-relative `.source` partition.
Edit/rebake changes strict save compatibility. Preview is schematic wire geometry,
not a DCC tool. Cook failures appear in the existing status/resource diagnostic.

### Controls

WASD/mouse, Space launch; **click** one hit-point impulse; **G/T** carry/drop/throw;
**B** low/high/off beam loading; **V** supported-frame load; **R** release its right
support; **P** spawn independent prefab; **C** explicitly remove an aimed rigid cell;
**F3** request/release annex, **H** adopt its family to root; **1/2/3** uniform/radial/game; **F5/F8** save/load;
**F9** reconstruct scene; **Esc** pause. Save completion must precede quitting.
The radial scene samples actual Judas radial gravity, plus a separate oblique uniform
zone. Fixed double absolute origins remain M23 coordinate representation, not rebasing.

## Identity, persistence and streaming

See the [public Fracture API](judasjs/fracture.md). Handles include instance epoch;
hit locations also include topology revision. Stale operations reject. Body/entity
handles stay normal generation-safe handles. `onFracture` runs after coherent
publication, with source keys and physical/explicit cause; reacquire current parts.
Destroying a family in a callback is safe. Restore never replays break notifications.

save-slot deformable participant is version 2 **only when fracture is present**, otherwise
version 1. Saves include tombstones, broken bonds, demand peaks, plastic rest,
physical child identity/motion/inertia reconstruction and owned fixed-joint warm
history. No global scene-fingerprint bump. Source identity remains content-based.
Pending cuts are not saved; capture records the last committed state. Required
resources/scripts must be initialized; capture defers rather than omitting state.
Existing save quotas remain in force; large unsupported saves fail normally.

A live region-owned family visibly pins its source region. Rigid descendants are
root-owned by default. `scenes.adopt(owner,"root")` removes the source-region pin;
the original authored member becomes a tombstone. True unload/revisit then neither
deletes travelling cells nor resurrects the authored family. An individually adopted
cell in another region retains that region while its external family owns it, even
if its collider is disabled. Whole-family destruction removes owned bodies/joints;
explicit cell removal is a durable tombstone; disabling retains topology/motion.
Liquid-bearing fracture owners are rejected at resource access/export with a clear
diagnostic; no conserved liquid is deleted. This is not liquid-cavity fracture.

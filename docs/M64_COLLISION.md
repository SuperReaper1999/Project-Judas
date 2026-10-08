# M64 — Mesh, convex and compound collision

**Status through M69:** M64 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Current candidate on M63 checkpoint `b5676438ed12d9cb3d05c634eaf5f578d7216ada`.
Human review is pending. Judas owns geometry/contacts; JavaScript owns skating and
other gameplay. No new dynamics engine, gravity implementation or skate component.

## Geometry and ownership

`.judascollision` is immutable CPU data in the normal AssetDatabase / asynchronous
ResourceManager. Bodies retain shared geometry; eviction cannot free a live shape.
Simulation/cooking need no GL. Renderer alone creates optional preview meshes.
World AABB broadphase selects bodies; a cooked local BVH selects triangles; normal
narrowphase/manifold/contact/impact/joint solvers resolve their actual surfaces.

| Target | Rigid contact | Public queries / motor |
|---|---|---|
| Static triangle surface | box, sphere, capsule, hull, supported compound children | ray, sphere, capsule, fixed box, closest surface; motor capsule |
| Static/dynamic convex hull | box, sphere, capsule, hull, compounds; sampled radial terrain | same queries; motor capsule |
| Oriented compound | independently placed/rotated box, sphere, hull children; one body | same child geometry, stable child keys |
| Radial terrain | existing sampled approximation, including hull vertex/face samples | existing approximate radial sampling; fixed-box cast rejected |

Triangle surfaces are **static**, open geometric patches, not watertight volumes.
One-sided is default: front contacts/casts only, outward authored winding. Two-sided
is an explicit cook option. Closest surface may inspect either side; containment
is unknown. Triangle volume sensors and dynamic concave bodies are rejected.
No continuous moving-concave-platform, remeshing or automatic decomposition.

## Cook and editor workflow

1. Import/register OBJ, embedded glTF or GLB as an ordinary model asset.
2. Assets → **Cook physical collider**: select the source and explicit ordinal.
   OBJ ordinal selects one object/group; glTF enumerates node/primitive pairs in
   file order, applying the chosen node's authored world transform.
3. Choose static triangles or convex hull, sidedness, additional positive source
   TRS and an assets-relative `.judascollision` destination.
4. **Cook and assign selected Body**, or cook/register and choose its ID in Body.
   Dynamic bodies require convex/primitive geometry; instance scale must be one.
5. Body → **Preview collision wireframe / normals / bounds**. Orange edges are
   physical creases/boundaries; blue edges are inactive internal features. Inspect
   hull recess filling: a convex hull deliberately fills concave source recesses.
6. Asset Browser reports source/settings staleness and rebakes saved settings.
   Failed cooking preserves the previous accepted file. Undo restores assignment.

CLI shares `LoadCollisionSource` / `CookCollisionFile` with the editor:

```sh
build/judas_collision_cook mesh source.obj physical.judascollision SOURCE_ASSET_ID 0
build/judas_collision_cook hull source.glb convex.judascollision SOURCE_ASSET_ID 1 2 1 1
```

CLI defaults to one-sided triangles; `--two-sided` opts in. It supports additional positive scale. The
editor additionally exposes sidedness/translation/rotation. No runtime cooking.
Separate low-detail physical sources are supported; rendering materials/LOD never
choose collision. Source file identity includes its authored glTF node transforms;
the stored cook transform is the **additional** selected TRS.

## Format and bounds

`JudasCollision 1` stores kind, sidedness, selection, exact-weld policy, source ID,
SHA-256 source/settings identity, additional transform, local double vertices,
triangles, source-feature keys, physical adjacency/active edges, balanced local BVH,
feature order, bounds, COM/volume/inertia, convex polygons and physical crease edges.
No native pointers. Text uses round-trip precision. Transactional `.cooking` write
is decoded before promotion. Loading checks bounded structure, bounds and winding.

Limits: 65,536 vertices/triangles; convex 128 vertices / 252 triangles; compound
64 children. BVH leaves ≤8 features, bounded 64-entry traversal stack. Query/contact
worklists are bounded by cooked topology limits; they can still be expensive for
huge overlapping geometry. Depth/iteration overflow reports an error, not a miss.
No simplification occurs. Choose an appropriate lower-detail physical source.

Importer position identities weld UV/normal splits without welding unrelated
coincident sheets. ID-free procedural sources opt into exact-position welding.
Proximity tolerance is **zero**. Duplicate/zero-area triangles are dropped with a
reported count; original triangle keys survive cleanup. Nonmanifold edges, inconsistent
adjacent winding and detected boundary T-junctions reject cooking. Correct source
connectivity rather than relying on a visual smoothing normal. Hull face keys are
generated, without a fictitious one-to-one source triangle map.

New positive nonuniform source scale is baked. Mirrors, shear, singular transforms
and non-unit cooked/mixed-instance scale are rejected. M23 remains fixed-origin:
absolute placements are reduced around the double origin before local float simulation.

## Internal edges and convex response

Adjacency classifies coplanar/concave shared edges inactive, actual convex creases
and open boundaries active. For two-sided back contacts, convex/concave creases
exchange roles; coplanar edges remain inactive on both sides. SAT still tests geometric separation; inactive internal
edges cannot supply a blocking solver edge normal. Clipped actual face witnesses
are deduplicated and reduced to a bounded four-point manifold. Cooked coplanar hull
triangles are full polygons, preventing half-face contact clipping.

Convex SAT uses actual face axes and physical edge cross-products; primitive fast
paths remain. Queries use geometric feature normals, which can differ from the
solver's edge-aware normal. No render-normal interpolation. A faceted curve remains
piecewise planar; tumbling/inelastic bodies can lose substantial mechanical energy.
M64 does not promise lossless curves or that JS skate assistance becomes unnecessary.

QuickHull is pinned to `4ef66c68950cb4db11d3b75bfe4034d807485ad0`, upstream Public
Domain; see [provenance](../third_party/quickhull/PROVENANCE.md). It is an offline hull
cooker, not a physics runtime. Hull mass properties use closed tetrahedral volume
integrals about the true COM. Authored pivot, visuals, joint anchors and recorded
motion remain in the original pivot frame via an explicit pivot-to-COM transform.
Compound density is uniform and mass geometry **sums** child volume/inertia with
rotation/parallel-axis terms; overlapping children count twice, not Boolean union.

## Queries and script inspection

See [JudasJS physics](judasjs/physics.md), [entities](judasjs/entities.md) and
[declarations](judas.d.ts). `Entity.collider` is a bounded detached value snapshot;
`physics.closestPoint(point, maximum, filter)` finds the nearest surface, with
containment separate. Casts expose child keys/features. Nothing exposes mutable
native geometry or copies the full mesh into JS. Layer/tag/ignored-generation,
enabled-state and sensor policy remain the existing M39/M44 contract.

Casts translate with fixed orientation, convergence tolerance 1e-5 metres, at most
256 advances per candidate. Concave candidates advance separately: a nearest
nonapproaching patch is not a separating plane of the whole mesh. Rays use actual
triangle intersections. Zero distance tests initial overlap; no all-hits API.
This is not a new universal high-speed/angular CCD guarantee.

## Other systems

- M42 remains body-pair observations: triangle feature changes do not create new
  semantic bodies/events. Compound children retain one rigid identity.
- CharacterMotor uses the shared casts/distance geometry; no skating rules.
- Navigation extracts **physical** triangles/hulls/oriented children in its local
  nav frame; fingerprints change with extracted geometry. Audio obstruction and
  M60 occlusion use ordinary authoritative rays, preserving real openings.
- M62 cloth/soft contacts use their existing sampled vertex/edge/face contact seam
  against new geometry. No exact triangle-triangle/self CCD or soft motor blocking.
- M63 rigid fragments retain ordinary primitive proxies and hit the mesh normally.
  M64 does not generate/remesh arbitrary fragment hulls.
- Streaming preloads cooked CPU data, installs at budgeted safe boundaries, retains
  shared dependencies and removes body/query registrations on unload. Adoption uses
  existing entity identity. Save restoration recooks nothing and creates fresh handles.
- M61 root saves include authored asset IDs/transforms and cooked identity in the
  runtime baseline. **Additive compositions still do not support disk saves**;
  the lab has a separate ordinary root-project configuration for cold-save proof.
- Export follows normal all-registered-runtime-assets policy, copies cooked and
  source identities, and rejects stale source/settings instead of inventing a box.

M54/M55 algorithms/cavity authority are unchanged. Rotated box-child geometry is
propagated through supported liquid calculations. Oriented box liquid mass geometry
must be disjoint; identity legacy compounds retain their accepted union quadrature.
Hull/mixed-child liquid loading, open-mesh capacity and legacy PBF contact/loading
without supported primitive geometry are explicitly rejected. No hull AABB buoyancy.

## Current collision lab

`projects/collision_lab/collision_lab.judasproj`: streamed lab.
`collision_lab_local.judasproj`: same assets/scene, root world for disk saves.
Scenes: `flat.judas`, rotated streamed `annex.judas`, `skate-mesh.judas` and
`skate-boxes.judas` (separate comparison geometry, no fallback under the mesh).

Green quarter-pipe: original R6, 32 curve facets, connected flat approach,
68 vertices/66 triangles. Gold doorway: actual hole, 12 vertices/10 triangles.
Red hull: asymmetric 8-vertex physical source. Compound keys 17/22/31 identify
rotated box/sphere/hull children. Small original cloth/soft blocks and ordinary rigid fracture cells
contact the curved mesh. Navigation and two ordinary spatial tones exercise its opening.

Controls: WASD/mouse; Space launch; click physical impulse; G carry/drop; T throw;
P hull prefab; C release a fracture interface; 1/2/3 lab/mesh-skate/box-skate; F3 request/release annex; H adopt region prop; F5/F8 save/load in
**root** configuration; F9 scene reload; Esc pause. Skate scenes start a four-ray
chassis automatically; V toggles only its seam velocity-transport compensation.
The unchanged reconstruction baseline is compensation-enabled; disabled comparison
retains its spring forces and other game behaviour. It is original reconstructed
content—not Claude's absent raw project or proof of rerunning that frozen game.

Measurements, failure/follow-ups, fingerprints and exact changed files:
[docs/evidence/m64](evidence/m64/). Human acceptance remains pending.

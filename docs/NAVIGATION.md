# Navigation surfaces, agents and dynamic edits

## Terrain-derived navigation


Navigation answers where an agent may go and proposes routes/steering.
**CharacterMotor resolves motion. Project JavaScript chooses the destination and
performs special traversal.** There is no engine EnemySystem, chase rule, input
policy, gravity policy or animation policy in navigation.

## Dependency and ownership

Pinned [RecastNavigation v1.6.0](https://github.com/recastnavigation/recastnavigation/releases/tag/v1.6.0),
zlib license. Unmodified upstream sources, license and provenance are under
`third_party/recastnavigation`; the full license is also in runtime export notices.
Recast polygonization is linked only into the editor/bake tool. Standalone uses
Detour, TileCache and the DetourCrowd adaptive avoidance sampler, not Crowd's
position integration. RuntimeWorld owns each mutable mesh, cache, query, agent
and link lifetime. ResourceManager asynchronously decodes immutable CPU layer
resources; independent worlds never share mutable navigation state.

## Authoring and baking

For exact scene keys and a complete SDK-only example, see the
[SDK authoring reference](SDK_AUTHORING.md).

1. In project settings, create stable navigation **areas** and agent **profiles**.
   Profiles specify radius, height, slope degrees and climb metres. Defaults retain
   compatibility for projects without navigation. Up to 62 lifetime area IDs
   (0..61) and 64 lifetime profile IDs; deleted IDs are retired.
2. Add a **Navigation surface** component. Its entity position/rotation defines the
   navigation frame; local +Y is internal traversal up, not world up or gravity.
   Choose profile, collision-source layer mask, bounds, cell/tile parameters,
   region threshold and simplification. Static enabled non-sensor collision
   geometry is the default source. Dynamic bake inclusion is explicit.
3. Add **Navigation modifiers** to classify bounded volumes by area or block them.
   `Exclude own collider source` is a separate source choice. Modifiers do not
   change physical collision. Overlapping volumes apply in stable entity-ID order.
4. Select the surface in Edit mode and click **Bake selected surface**. The normal
   registered asset is `Assets/navigation/surface-<entity ID>.judasnav`. The inspector
   reports current/STALE/error and tile-layer count. **Clear baked reference** removes
   the scene reference while retaining the asset for other users.
5. **Preview baked polygons** displays an Edit-mode snapshot. The Debug menu's
   **Navigation mesh / routes** also shows current Play-mode polygons, carved holes,
   area colours, purple link segments and yellow agent paths. Default area is cyan;
   non-default areas purple. Tile/polygon edges are visible. Re-preview after edits.

Headless authoring uses the same baker, through CMake's `judas_navigation_bake`:

```sh
./build/judas_navigation_bake /path/to/YOUR_PROJECT.judasproj Scenes/YOUR_SCENE.judas
```

Replace both paths with your project file and a scene path relative to its project
root. The tool bakes authored non-terrain surfaces, registers or reuses asset IDs,
writes scene references, and reports layer and polygon counts, time and source
fingerprint. Source-owned terrain navigation is prepared automatically during
Play. For published bakes, use `--check-dependents` to identify stale inputs
and `--rebuild-dependents` to refresh owned outputs.

## Geometry, coordinates and staleness

Bounds are in the surface frame. Queries and steering use local world-space
metres. A large absolute origin does not alter the local bake geometry or identity.
On a runtime origin shift, navigation translates active and staged surface frames,
obstacles, links, agent destinations and route corners. The baked polygons remain
in their surface-local coordinates.

Enabled physical boxes/compound boxes are tessellated exactly. Spheres use a finite
24×48 collider tessellation. Terrain uses the real TerrainLibrary surface's 96×192
mesh approximation. Render meshes/materials do not define navigation. Physical
scale follows the existing collision representation, not visual entity scale.
Obstacle components are excluded from permanent source geometry and carved separately.
Modifier boxes are conservatively projected into each surface frame.

Source/settings/profile/registry content produces a path-independent SHA-256.
Changing relevant authored source/settings makes the bake stale. The fingerprint
is intentionally conservative: project registry edits and included geometry outside
a surface's bounds can require a rebake. The editor displays staleness; runtime
rejects stale data with diagnostics rather than using it; export rejects it.
Moving a runtime obstacle updates carving, not this authored source fingerprint.
Move/rebake authored surfaces in Edit mode; moving live navmesh surfaces is unsupported.

## Runtime components

| Component | Responsibility |
| --- | --- |
| NavigationSurface | Reference one accepted profile-specific bake in its frame. |
| NavigationAgent | Track destination/path, corners, remaining distance, link state and advisory steering. |
| NavigationObstacle | Box/cylinder TileCache carving; independent of a physical Body. |
| NavigationLink | Local endpoints, direction, area and tolerance; explicit script traversal handoff. |
| NavigationModifier | Bake-time area/non-walkable volume and source exclusion. |

All use normal scene properties, prefab overrides, asset IDs and entity identity.
Runtime prefab agents are independent. Disabled/destroyed agents lose their route;
world replacement, authored reset and Stop discard caches and guidance. JS handles
reacquire the ordinary live entity; disabled/stale control calls fail safely.

Profiles must match the surface bake. Authors must choose profile clearance that
conservatively covers their actual mover; navigation does not resize its collider. Areas have include/exclude masks and per-area
costs >=1. Detour resolves polygon corridors and weighted routes; explicit link
endpoints compose otherwise disconnected surface segments. Partial and failed
routes are distinct from arrival. No raw polygon references reach scripts.

Guidance reads actual motor/body poses before fixedUpdate. A motor agent's position
is its capsule foot, including its rotated local shape offset, not its root pivot.
Other movers use their observed root point. Progress projects onto nearby navigation
space so a vertical pivot offset does not prevent corner advancement. JS submits
desired velocity to CharacterMotor.
The agent does not write transforms. Detour adaptive avoidance observes nearby
agents and current polygon wall segments, with 32 neighbours. It is advisory and
does not promise perfect crowd separation or motor-to-motor physical blocking.

TileCache installs/removes/moves box/cylinder obstacles incrementally. Paths can
remain temporarily stale until affected tiles finish rebuilding and agents repath.
Use modest slow blockers, not every fast-moving body. Boxes carve a conservative
surface-frame AABB; cylinders align with surface up. Physical collision must be
changed separately if a game also opens a physical barrier.

At a link entry, steering stops and state exposes the safe link and exit point.
JS decides how to cross, then calls `completeLink()`. No engine jump/ladder/teleport.

## Assets, persistence and export

`.judasnav` (`JudasNav1`) stores profile/configuration, source fingerprint and tiled
heightfield layer bytes, with bounded decode checks. Metadata/lengths use explicit
encoding; layer payloads use pinned Detour's current Linux native format, not a
cross-platform serialization promise. Runtime reconstructs polygons with TileCache,
without Recast or source meshes. Tiles are stored independently; streaming is future work.

Project format remains 1 with an optional navigation configuration; scene format
remains 3 with ordinary `nav.*` component properties. Canonical fingerprint schema
remains 5: conditional navigation configuration and baked-content extensions distinguish
navigation worlds, while worlds without navigation retain their prior identity.
Runtime routes/caches are transient, not save objects. Baked bytes contribute to the
effective authored baseline without export-machine paths. Normal export registered-asset
export includes bakes, project settings, scripts/prefabs and dependency notices.

## Limits and validation

Recast is locally 2.5D. Arbitrary world/surface orientation is supported; a single
surface is not continuous whole-planet navigation. Independently oriented surfaces
and explicit links are supported; planetary tiling/streaming is deferred.
No flying/swimming-volume navigation, runtime full-world baking, AI architecture,
perception, combat planner, animation controller or world streaming is added.
Current bounds: 4096 bake-grid tiles, 16384 serialized layers, 256 obstacles/surface,
4096 query nodes, 512 corridor/corner buffers; buffer-limited routes are partial.
Nav raycast is a local-horizontal traversal test, not a physics/visibility query.
Its returned height interpolates query endpoints; sample to obtain surface height.
Links follow authored/explicit transform writes, not automatic dynamic-body motion.
Surface enable state is authored; runtime toggles cover agents/obstacles/links.

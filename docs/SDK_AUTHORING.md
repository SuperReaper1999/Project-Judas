# Versioned SDK authoring reference

Use this copy with its SDK BUILD_INFO.json. This reference describes the current
Terrain V2, JudasScene 3 text/named documents and native navigation baker.
Do not use old V1 terrain create/brush/cook commands or liquid-bake tools.

## Terrain → navmesh → NPC

Create a landscape in **World → Terrain tools → New landscape**, then sculpt,
paint or excavate it. Save and update playable terrain publishes the native
asset, preview and source receipt together. Keep the owner root, unit-scale and
static. See [Terrain V2](TERRAIN_V2.md) and the editable
[Highlands Workshop](../projects/terrain_v2_workshop/README.md).

From an existing project's root:

```sh
mkdir -p Authoring Assets/Terrain
"$SDK/judas_terrain_author" v2-new Authoring/yard.judasterrain2 highlands 65 0.5 19 2
"$SDK/judas_terrain_author" v2-native Authoring/yard.judasterrain2 Assets/Terrain/yard.judasmodel
```

`v2-new` takes source, generation kind, sample count, spacing, seed and amplitude.
`v2-native` writes a native `.judasterrain`, preview model/texture and recipe.
Register the files using the editor Asset Browser. Use the generated metadata's
IDs, never another project's IDs. The terrain entity's exact binding is
`terrain.asset "NATIVE_ASSET_ID"`. Its render preview may reference the model and
texture, but live geometry, collision, excavation and paging belong to that
native terrain owner. There is no independent full-mesh collision bake to keep
synchronised. An empty `nav.surface.asset` on this same owner selects source-owned
live navigation; source publication updates it alongside collision.

For independent static scene geometry, add a separate Navigation Surface and bake
it with the SDK tool. The complete surface keys follow. Unlike a native terrain
surface, independent bakes need rebuilding when included geometry changes.

### Navigation surface

In the Inspector add **Navigation surface**. Put these exact fields inside its
text scene object. These are working example settings, not universal defaults.
Keep all fields, including the blank asset before first bake.

```text
  nav.surface.enabled "1"
  nav.surface.includeDynamic "0"
  nav.surface.profile "0"
  nav.surface.sources "18446744073709551615"
  nav.surface.halfExtents "16 6 16"
  nav.surface.cellSize "0.2"
  nav.surface.cellHeight "0.1"
  nav.surface.tileSize "32"
  nav.surface.minRegion "8"
  nav.surface.simplification "1.3"
  nav.surface.asset ""
```

The entity transform places and orients the bounds. Local +Y is traversal up.
Half extents and cell dimensions are metres. Tile size is in cells. The baker
squares minRegion to get its minimum region area in cells. The editor defaults
match the block above except for half extents, which default to 32 8 32. Simplification is the contour error in voxel units.
Profile is a numeric project profile ID. Set the profile's radius, height,
slope and climb to match the NPC. Sources is a 64-bit collision-layer mask.
Preserve the quoted decimal string when generating scene text in JavaScript.

Click **Bake selected surface**, or run
`"$SDK/judas_navigation_bake" Workshop.judasproj Scenes/main.judas`.
The CLI bakes all authored surfaces in the scene, registers the generated
Assets/navigation/surface-ID.judasnav assets and writes their IDs into the
scene. Inspect **Preview baked polygons** in Edit mode. Rebake after sculpting,
changing included static collision or changing bake settings. Painting alone
does not invalidate the bake. Render-only geometry does not contribute.

### NPC movement

Add a **Character Motor**, **Navigation agent**, and script attachment.
Its agent uses these exact scene keys:

```text
  nav.agent.enabled "1"
  nav.agent.avoidance "0"
  nav.agent.profile "0"
  nav.agent.areas "18446744073709551615"
  nav.agent.costs ""
  nav.agent.speed "2"
  nav.agent.arrival "0.25"
  nav.agent.cornerDistance "0.25"
  nav.agent.repathSeconds "0.5"
```

Use the same profile as the surface. Areas is a 64-bit mask; costs is a
space-separated sequence of area ID and cost pairs. Speed is m/s, arrival and
corner distance are metres, and repathSeconds is seconds. The example disables
avoidance for its single courier. Editor defaults enable avoidance and use
3 m/s speed and 0.3 m arrival distance. The other agent values match the block.

In fixedUpdate, wait for a usable route, set the destination, then feed
`entity.navigation.steering` into `entity.character.velocity`. Preserve the
velocity along the motor's up vector so the controller can resolve gravity and
contacts. Handle missing, partial and invalid paths separately from arrival.
If using Utility AI, let its action choose the destination and use the same
movement path. See the [navigation API](judasjs/navigation.md) for route state
and custom links.

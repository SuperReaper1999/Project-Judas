# SDK authoring reference

Use the copy shipped with your SDK. Check its BUILD_INFO.json for the engine
version, source revision and channel. This page describes terrain cook format
M75-1, JudasScene 3 text authoring, and the navigation bake used by this SDK.
The baker may rewrite the scene in the structured format. Keep the authored
source under version control and let the SDK tools write generated assets.

## Experimental Terrain V2 preview

The source-build Alpha V2 preview additionally supports native sampled/volumetric
landscapes. Use the current [Terrain V2 guide](TERRAIN_V2.md), including its
geographic native-patch workflow and `terrain.planet` scene reference. It reuses
this navigation system through source-owned live surfaces; the V1 baked-mesh
recipe below remains valid for existing projects. Do not assume these preview
formats exist in older published SDKs or replace a project's assets automatically.

## Terrain → navmesh → NPC

Start with the [Terrain Navigation Workshop](../projects/terrain_navigation_workshop/README.md).
Its scene has complete object blocks, including transforms, static triangle-mesh
collision, a navigation surface, and a scripted CharacterMotor courier. The
recipe creates and sculpts terrain, installs its asset IDs and bakes navigation.

### Terrain source and cooked scene fields

In the editor use **View → Terrain and world tools → Terrain sculpt / paint**.
Create a project-owned Imports/*.judasterrain source, cook it, then choose
**Create terrain instance**. Keep the terrain instance at unit scale.
Select it and use **Edit selected terrain** to sculpt. Save the editable source
and cook again to publish render and collision products.

The CLI equivalent, run from the project root:

```sh
mkdir -p Imports
"$SDK/judas_terrain_author" create Imports/yard.judasterrain 65 65 32 32 Assets/Terrain/yard
"$SDK/judas_terrain_author" brush Imports/yard.judasterrain raise 0 0 8 3 0.6
"$SDK/judas_terrain_author" cook Workshop.judasproj Imports/yard.judasterrain
```

Create takes X/Z sample counts (2–257), width and depth in metres, and the
product directory. The raise brush takes local X/Y/Z, radius in metres and
height change in metres. The cook reads the project-relative source path.
Use the `id` strings from the generated `.judasmeta` sidecars. Asset IDs are
project references, so don't copy another project's IDs into these fields.

| Scene key | Value |
| --- | --- |
| `render` | `mesh` |
| `render.mesh-asset` | ID from terrain.judasmodel.judasmeta |
| `render.texture-asset` | ID from terrain.tga.judasmeta |
| `body` | `static triangle-mesh` |
| `body.collision-asset` | ID from terrain.judascollision.judasmeta |

Copy the full terrain object from the workshop for the accompanying required
render/body fields. The source itself stays in Imports; the scene references
the cooked outputs. This heightfield recipe covers a flat world with sculpted
heights. Use [volumetric terrain](M84_VOLUMETRIC_TERRAIN.md) for overhangs.

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

Add a **Character Motor**, **Navigation agent**, and script attachment. The
workshop's Courier object shows the complete `motor.*` and `script.0.*` fields.
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

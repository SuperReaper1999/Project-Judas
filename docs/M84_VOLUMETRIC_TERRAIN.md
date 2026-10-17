# M84 — editable volumetric terrain

M84 authoring capability. M97 adds a separate opt-in per-instance [runtime editing path](M97_LIVING_WORLD.md) using these same geometry operations. Linux checks are recorded in
[evidence/m84](evidence/m84/REPORT.md); the operator accepted M84 on 15 October 2026. Windows validation remains outstanding/nonblocking.
See [acceptance record](evidence/m84/HUMAN_ACCEPTANCE_2026-10-15.md).

## Open and use the lab

Open `projects/volume_terrain_lab/volume_terrain_lab.judasproj` in the editor.
The large rock contains a through-tunnel, a widened chamber, an overhanging roof
and a sealed pocket. These are actual inward-facing collision surfaces, not holes
painted into a heightfield. The small exterior apron is a separate ordinary body;
it does not replace the tunnel's excavated floor.

Play controls: **WASD** move, mouse look, **Space** jump, **E** throw a ball,
**F1/F2** flat/rotated scene, **F3** request/release the retained annex,
**F6/F7** save/load, **R** reload, **Escape** pause.
The rotated scene transports geometry, character, gravity and navigation together.
The annex has its own ordinary authored gravity region.

### Editing workflow

1. Stop Play. Select **Editable volumetric rock** in the hierarchy or viewport.
2. Open **View → Volumetric terrain - dig / build**.
3. Click **Edit selected volume**. This resolves the selected mesh's recorded source.
4. Enable the viewport brush. Choose **Remove**, **Add** or **Smooth**; set radius and strength.
5. Drag on the surface. A depth offset moves the brush centre along the view ray.
   Use the explicit local centre to cut an enclosed chamber without first opening it.
6. Release to commit one undoable gesture. Escape cancels; focus loss ends the gesture.
   Undo/Redo applies to the volume draft while the brush is active.
7. **Save source**, then **Cook geometry + collision**. The cook runs on the job pool.
8. Inspect navigation dependencies and rebuild stale owned navigation from saved scenes.
   Save scene changes before the batch rebuild. Play/export reject stale volume products.

New sources can be created as bounded solid blocks. **Fork** creates new source/product
identities; duplicated instances intentionally share an immutable cooked asset.
Published mod kit sources are read-only: fork to an owned Imports path first.
The source draft is separate from scene Undo and scene Save. Unsaved source changes and
active cooks block Play, project replacement and export so edits are not silently discarded.

## Representation and ownership

`VolumeTerrainSource` is a project-owned scalar grid in asset-local metres:
negative values are solid, positive values are air; zero is the extracted surface.
It approximates a signed distance field, but is not guaranteed to remain an exact distance
function after composition or smoothing. It is geometry data, not fluid quantity or physics mass.

- Immutable positive outer samples enclose each finite volume and prevent an open boundary.
- Remove is sphere subtraction: `phi' = max(phi, -sphereDistance)`.
- Add is sphere union: `phi' = min(phi, sphereDistance)`.
- Composition updates a bounded brush stencil with a two-cell-diagonal halo; remote samples
  remain unchanged. Strength blends toward that local composed field; smoothing blends six
  neighbours inside the brush. This is a local editing field, not global distance reinitialization.
- A gesture schedules dabs by 3D arc length, not frame/event count. Stationary hold does not
  repeatedly accumulate edits. Intermediate samples and undo memory are bounded.
- Each cell uses six consistently oriented tetrahedra. Shared crossing edges are indexed
  deterministically; triangle winding points from solid into air. Normals derive from the
  local field gradient, with geometric fallback rather than a gravity assumption.

The source is `Imports/*.judasvolume` (version 1 JSON). Cooking uses the existing model
archive, collision cooker, asset IDs and rollback-capable authoring publication:

```
editable scalar source
  → indexed surface mesh
  → ordinary .judasmodel + .judascollision + content receipt
  → ordinary scene / prefab entities
  → PhysicsWorld + CharacterMotor + Recast/Detour + rendering
```

There is no new runtime asset type, parallel terrain physics solver, runtime density allocation
or terrain-specific gravity. Only existing cooked assets load in games. Authoring code links
into editor/tools; games that do not use volumetric assets incur no volumetric simulation cost.
Gravity remains an independent field sampled by existing consumers. Unit-scale rigid placement
is required by the existing cooked static-triangle collision contract.

## Publication, navigation, saves and streaming

A cook checks saved source bytes and stable product identity ownership, captures expected
outputs before work, supports cancellation, and rejects external changes before publishing.
A content receipt binds source and model/collider hashes. Editor Play/export validate it;
packaged runtime needs only cooked products, not the source or authoring tools.

Preview extraction is coalesced on the job pool. Renderer uploads completed immutable meshes
on its own thread. While dragging, the displayed/picked preview may trail the latest field
revision; it catches up without running extraction in the input loop. An empty current preview
hides the old cooked mesh rather than pretending that deleted material still exists.

Navigation uses the emitted real triangles, including underground surfaces and multiple height
layers. Geometry edits invalidate the existing navigation fingerprint. Rebuild uses the normal
M53/M76 baker/dependency workflow; it is explicit, not an unbounded background runtime rebake.

Full saves and retained regions preserve the ordinary collision/model references and physical
entity state. Reloading resolves the same edited cooked asset. Source edits live in the authoring
file and cooked products, not in a second runtime terrain ledger. Independent prefab instances
use existing hierarchy/asset identity mechanisms.

## CLI / reproduction

```sh
build/judas_volume_author create /path/to/project/Imports/rock.judasvolume 33 25 33 16 12 16
build/judas_volume_author brush /path/to/project/Imports/rock.judasvolume remove 0 0 0 2 1
build/judas_volume_author cook /path/to/project/game.judasproj Imports/rock.judasvolume
build/judas_volume_author validate /path/to/project/Imports/rock.judasvolume
build/judas_volume_author fork /path/to/source.judasvolume /path/to/new.judasvolume
build/judas_navigation_bake /path/to/project/game.judasproj --rebuild-dependents
```

`tools/CreateVolumeTerrainLab.py` reproduces the ordinary current project using the same cook.
Focused tests: `judas_volume_terrain_tests`, `judas_volume_application_tests`.
The application test takes `PROJECT SCENE watch|write|read OUTPUT [EXPECTED_PHYSICAL_FILE]`.

## Deliberate limits

- This M84 workflow edits source assets outside Play. Live per-instance gameplay editing
  now uses [M97 Terrain](judasjs/terrain.md); it does not modify these source files.
- Bounded dense grids: 3–129 samples per axis, at most 300000 samples, .3–1024 m extent per axis.
  The editor's cubic creation control is capped at 37 samples so its initial solid fits the collision budget. No seamless chunk stitching,
  adaptive octrees, runtime remeshing, planetary-scale volume store or LOD is implemented.
- At most 200000 extracted vertices and 65536 triangles per product. Over-budget/cancelled
  cooks retain last good products and report failure; they do not silently reduce resolution.
- Feature fidelity follows grid spacing. Sub-cell cracks/thin walls can disappear or merge.
  Tetrahedral extraction can show diagonal facets; this is not smooth industrial CAD.
- The solid is static collision, not simulated soil, granular flow or collapsible rock.
  Removing material does not generate physical debris or maintain a material mass ledger.
- An entirely empty source can be edited/previewed, but cannot publish an empty collider;
  remove the scene instance or add material before cooking.
- Brush editing currently requires unit scale and rigid placement. A transformed instance
  edits the shared local source; fork if only that instance should change.
- Source files are not included in runtime exports. Use the authoring project to edit them.

## Human checklist

1. Walk through the tunnel, beneath the roof and into the chamber; throw balls against it.
2. Stop, select the rock, activate the volume tool and excavate a new opening.
3. Add material, smooth it, Undo/Redo and cancel a pending gesture.
4. Cut an enclosed pocket at an explicit local centre, then connect it to the tunnel.
5. Save/cook/rebuild navigation; confirm the new passage is physical in Play.
6. Reload/save/load; request/release/revisit the annex and check edits remain.
7. Try the rotated scene and brush on its rotated instance.
8. Repeat traversal and object interaction in the moved standalone.

### Existing motor overlap limitation observed during review

The original cluttered rotated-cave fixture can intermittently report unsupported while
standing above a valid floor. The unchanged motor's first support sweep hits the side of
crate 20 at zero distance (local normal approximately `[-.773,0,-.635]`) before the
walkable floor. Gravity remains correct and a separate floor query still hits the real
terrain about .926 m below the capsule centre. Original scenes and traces are preserved
in M84 evidence. No controller, filtering or physics semantics were retuned here.
The current demonstration starts its crates off the traversal lane and exercises a
script-thrown crate inside the cave separately. This is a recorded existing motor
support-selection limitation, not a claim that crowded overlap behaviour was repaired.

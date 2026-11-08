# Geographic native terrain — integration qualification

This is an intermediate Terrain V2 checkpoint on Experimental, based on739805be.
Main and supported releases remain unchanged. The Alpha fluid lock, installed RPG
SDK, existing consumer assets and independent research are not changed. No human
acceptance is claimed. WORKING_STATE.md tracks remaining whole-programme work.

Source3/native envelope2 and `terrain.planet` now carry geographic placement,
immutable provider identity and protected rim samples. The shared source/mesher
produces both collision and rendering; ordinary world-metre brushes and solid
occupancy use the same map. Source2/native1 retains its old encoding and behavior.
Old flat-only readers reject envelope2 explicitly. Root patches cannot be moved,
scaled or turned into prefabs. Non-overlapping private region reservations retain
old base support until publication; hiding/removal restores retained ground.

The existing landscape creator gains Planetary patch, deriving pitch/placement
from the parent and current scene focus. Its existing undo, source editing,
publishing and Play guards remain. TerrainPlanetPreview owns Scene-view masks and
header caching outside the main frame loop. Inspector navigation derives local
mapped bounds rather than using absolute altitude. CLI `v2-planet` feeds the same
source and `v2-native` pipeline; old full-mesh/script bootstrapping rejects a
projected source. No competing terrain runtime or solver is introduced.

## Corrections demonstrated during integration

- The single-entity schema validator renumbered the owner to1 and accidentally
  converted an external planet1 into a self-reference. It now preserves the
  reference kind with a separate validation identity; binding still checks the
  actual endpoint. `schema-reference-failure.log` preserves the failure.
- Retained regional restoration translated an absolute planet descriptor into
  the simulation frame. Initial placement, cached scene translation and cold
  restoration now consistently retain its descriptor frame. The rotated region,
  origin change and independent cold process exercise the correction.
- A negative control proved private state restoration could detach a geographic
  source from its footprint. Private/public state setters now preserve placement;
  invalid initial binding also rejects before publication. The failed control is
  retained; the final component suite passes40checks.

## Qualification

Component40/cold9; actual streaming37/cold14; existing component35/surface59/
world93 (300 exact seam steps)/interaction85/preparation25/streaming69;
PlanetWorld220/PlanetTerrain10074/scene fingerprint140; generation23/export18.
Editor creation, viewport excavation, publication, Inspector-authored navigation,
Play and base-mask checks pass23 on desktop NVIDIA GL4.6 and portable3.3 each.
Existing fresh-project creation passes46. Full editor author/undo/PlayStop keeps
the copied authored scene IDENTICAL. Its edit capture is obscured by the
existing World building automation dialog; it is workflow evidence, not a visual
acceptance image. The dedicated preview capture exercises the actual native pit.
API contract passes. Registration11passes with2unchanged environment skips.

Both normal and planetary Workshops pass genuinely relocated exports from foreign
working directories while editable sources are parked. Ordinary controls raise,
paint, cut a tunnel, wait for actual asynchronous completion, walk through it,
save, quit and cold-load. Both restore8volume chunks,2surface dabs,empty queue,
ready collision/navigation and a player beyond the hill. Planetary restored z is
-10.380638; ordinary -10.608723. See relocated/*/RESULTS.json and actual images.
A separate planetary save after live O-origin control cold-restores origin
(0,1024,0), paint and excavation intact. Scripts use absolute queued brush points.

## Measurement

measurements/ contains180 raw samples per phase of the actual game's full render
path at1280x800, Release GNU15.2, Ryzen5600/8GB/GTX9704GB/NVIDIA580.178.04.
Intact median: simulation/scripts/publication .4093ms, render CPU2.315ms,
GPU elapsed1.289ms. Excavated median: .8970ms/2.327ms/1.363ms respectively.
p95 and worst values, draw/triangle counts and peak process RSS are machine-
readable. This is one bounded scene/camera and not an optimization comparison.
CPU/GPU scopes overlap; GPU elapsed includes submission bubbles and queries are
synchronous. Do not add these numbers to invent FPS or general performance.
The game publication worker reconstruction during cold load was about107ms,
with owner-thread publication about .14ms; not a per-brush deadline guarantee.

The normal scripted application harness renders only requested screenshots. Its
GPU pairs did NOT resolve, so attempts/screenshot-harness-profile.json is retained
but excluded from GPU/FPS conclusions. The first dedicated benchmark omitted the
normal AdvanceRuntimeTerrain boundary, leaving a pending product; that harness
failure is retained and corrected by invoking the same public boundary API.
No terrain policy, wait gate, physical quality or expected outcome was weakened.
Other retained setup failures: coordinator-only capture of an active region
without its world snapshot, missing generated-project settings before texture
lookup, a missed cache-clear call after editor extraction, and mixed-provenance
export correctly rejecting an out-of-date runtime. Final matching builds pass.

## Boundaries and human checks

Interior cube-face patches only,32..512cells matching physical/visual rim budgets.
No cross-face excavation, overlaps, movable prefab patches or automatic migration
of changed providers. All active source collision chunks remain resident (max1024).
Mapped patches render native meshes; the surrounding planet retains its LOD.
Runtime source arrays are resident, not disk-demand-paged. Final physical-interest,
large-world presentation and fresh-SDK qualification remain broader work.

Open projects/terrain_v2_planet_workshop/terrain_v2_planet_workshop.judasproj with
its matching Experimental editor. PressT, wait for cuts, walk through; try sculpt,
paint, add/remove, O-origin, F6/quit/relaunch/F7. In Edit mode create another patch,
try source undo, publication and scene Save, then Play/Stop. Inspect protected
boundaries and explicit overlapping/changed-provider diagnostics. Human visual
and gameplay acceptance remains pending; do not promote main automatically.

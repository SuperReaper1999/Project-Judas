# Planetary replacement core — intermediate Terrain V2 work

Base:6315daadfc2de8e35c8f8d92b0e625b2df97633a, Experimental.
Source hashes below identify the actual core; no new release is created.

```sh
cmake -S . -B build
cmake --build build -j2 --target judas_terrain_v2_planet_chart_tests judas_terrain_v2_planet_render_tests judas_planet_terrain_tests judas_planet_world_tests
build/judas_terrain_v2_planet_chart_tests
build/judas_planet_terrain_tests
build/judas_planet_world_tests
JUDAS_GL_PROFILE=modern build/judas_terrain_v2_planet_render_tests FRESH_OUTPUT
JUDAS_GL_PROFILE=portable build/judas_terrain_v2_planet_render_tests OTHER_FRESH_OUTPUT
python3 -m unittest tests.test_validation_registration
```

Use one build at a time. Native GPU runs used real desktop NVIDIA GTX970/580.178.04,
GL4.6modern and GL3.3portable; host Ryzen5 5600/8GB RAM. These are not Android,
browser or Windows qualification. Both new tests are registered in suites.json.

Actual acceptance: geographic chart/normal transforms on all6faces; independent
GridPoint/finite-difference controls; distant origins; local geometry at12km
height; strict collision cooks and cross-chunk curved hill tunnel with an uncut
blocking control. A separate actual PlanetTiles owner initially blocks an excavated
pit, then its geographic override exposes the native floor. Physical interest
crosses the rim, malformed/budgeted cutouts preserve old state, and removal
restores retained base support. Backing colliders remain allocated but disabled
inside the replacement; no memory saving is claimed. Triangle diagnostics count
only unmasked base draws. Ordinary selection is exact against the frozen accepted
function in tests/fixtures/terrain_v2/planet_selection_reference.inc over20cases.

GPU tests independently check main-frame pit depth, hidden base removal, shadow
base removal, and sampled native/base rim framebuffer depth against MK2 queries.
343checks per path, max difference7.9968e-6m. Captured plain-colour pit and rim
images were inspected; they are geometry fixtures, not attractive terrain art or
human gameplay acceptance. The ordinary textured Workshop remains separately
qualified in prior checkpoints. Core test timer was renamed fixture_work_ms:
it includes all subsequent negative controls/world composition/selection work,
not just preparation; old logs retain the earlier misleading label with this
scope correction. No solver or physics expectation was weakened.

Preserved failed attempts: initial test omitted PhysicsWorld::Init and crashed in
AddBody (native gdb confirms); correct test setup fixed it. Restricted gdb could
not attach; native run recorded the stack. First build needed CMake regeneration.
An initial new indentation warning was corrected; later frozen builds are clean.
These are harness/build issues, not newly established terrain engine defects.

CRITICAL: normal projected source assets, scene/planet binding, authoring,
world-metric brushes, receiving solid fields, cold-save/streaming reconstruction
and the actual playable planet Workshop are NOT implemented by this checkpoint.
Existing planet world cold-save controls pass for the existing unmasked path.
Do not claim new cutouts persist automatically. Initial cutouts require interior
cube-face tiles, nonoverlapping footprints and compatible/budgeted protected
rims; cross-face excavation is not yet supported. Main, fluid lock, supported
releases, consumer games and installed SDKs remain unchanged.

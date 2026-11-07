# Source-owned solid and cross-system controls

```sh
cmake --build build -j2 --target judas_terrain_v2_interaction_tests judas_terrain_v2_world_tests judas_terrain_v2_surface_tests judas_terrain_v2_component_tests judas_game_water_tests judas_game_water_runtime_tests judas_game_water_script_tests judas_particle_world_tests judas_particle_collision_tests
build/judas_terrain_v2_interaction_tests
build/judas_terrain_v2_world_tests
build/judas_terrain_v2_surface_tests
build/judas_terrain_v2_component_tests
build/judas_game_water_tests
build/judas_game_water_runtime_tests
build/judas_game_water_script_tests
build/judas_particle_world_tests
build/judas_particle_collision_tests
```

Use an Experimental build. Alpha continues to reject advanced execution; the
interaction test explicitly reports NOT EXERCISED when that policy applies.

The reference uses the actual unmodified triangle boundaries with the old
nearest-shell interpretation, not fabricated volume data. It reports240m3 free
where independent rectangular volume accounting requires192m3. The corrected
source-owned path reproduces192; digging opens capacity without creating water.

Early fixture failures are preserved. The first diagnostics accidentally selected
default radial gravity at the origin for an analytic uniform-gravity box. The
existing curvature guard rejected it correctly. Another new negative-control
expectation assumed the shell would leave more than240m3; measured capacity was
exactly240, because the existing voxel classification also seals ambiguous cells.
The physical192m3 oracle never changed. The final negative control requires the
old path to disagree materially with that unchanged answer. No production fluid
solver tolerance, damping, pressure or transport test was altered.

The tiny2048-cell interaction records one edit-preparation cost and30 subsequent
steady preparation scopes; those exclude pressure/transport and are diagnostic,
not a general speedup or frame-rate benchmark. The geometry cache avoids rebaking
for painting, and stable source ownership survives replacement/origin/restoration.
A snapshot queries its immutable source, never live physics per water sample.

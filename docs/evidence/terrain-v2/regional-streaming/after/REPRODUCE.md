# Native regional preparation and restore

Base: `5a7cf7df`. Functional source hashes and the eventual checkpoint distinguish
this correction from the behaviour-preserving extraction. Release/j2, Ryzen 5
5600, 8 GB RAM; CPU-only regional tests. Do not run compiler jobs concurrently
with timings. Native application tests use the desktop GTX 970.

```sh
cmake --build build -j2 --target judas_terrain_v2_preparation_tests judas_terrain_v2_streaming_tests judas_terrain_v2_surface_tests judas_terrain_v2_world_tests judas_terrain_v2_component_tests judas_terrain_v2_interaction_tests
build/judas_terrain_v2_preparation_tests
build/judas_terrain_v2_surface_tests
/usr/bin/time -v build/judas_terrain_v2_streaming_tests --write /tmp/terrain-native-FRESH
build/judas_terrain_v2_streaming_tests --read /tmp/terrain-native-FRESH
build/judas_terrain_v2_world_tests
build/judas_terrain_v2_component_tests
build/judas_terrain_v2_interaction_tests
```

Use a fresh path for write, the same path in a genuinely separate read process.
The original 67/13 physical/persistence checks are retained; two additional checks
verify actual resident payload accounting and its update after runtime edits.
The same lattice, 1,024 collision chunks, sculpt, excavation, save data and world
origin are used. Largest integration-unit time is a cumulative maximum, not a
frame percentile. Source/mesh estimates exclude BVH. Accounted native payload
includes BVH and LOD but not allocator/physics-world/GPU storage. Process RSS is
reported separately. No memory improvement is claimed from the small RSS change.

Preparation controls cover real worker preparation, cancellation, wrong source,
invalid chunk coverage, double consumption, private staging, physical publication,
exact geometry certificate and release. Both native pending and live budget
failures use actual region preparation and assert no bodies/source publication.
The first failed invalid-coverage control exposed a static identity-seed rollback
leak: registration allocated its body before creating an EntityRecord. Cleanup
now also uses the already-owned category handle on that failure path. The failed
output is retained, not replaced by the passing runs.

Surface controls extend the original 44 with multiple native pages, reverted
height samples, independent retained paint, corrupt required-page rejection and
paint-only restoration preserving all collider IDs. The original 300-step seam
comparison against monolithic geometry remains exactly zero.

`handoff-only` is an intermediate experiment: worker handoff fixes initial
registration, while revisits still recook the whole source. Final selective
restore removes that second preparation, retaining immutable base identity checks.
`03` is the first complete corrected qualification. Subsequent frozen runs also
include cancelled-product payload accounting and avoiding optional terrain
allocation in ordinary streamed projects. See logs rather than assuming all
prototype binary hashes identify the final source.

For the actual relocated workshop and separate-process save:

```sh
python3 tools/validation/qualify_terrain_v2.py --build build --output /tmp/terrain-relocated-FRESH
```

Run on the real desktop graphics session. This creates its own copied editable
project, cooks native terrain, exports, relocates the package, parks its source,
excavates/traverses/sculpts/paints and verifies state in a fresh runtime process.
Existing games and the installed RPG SDK are not modified. Automated screenshots
and gameplay input are engineering verification, not human acceptance.

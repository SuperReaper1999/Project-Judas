# Runtime surface qualification

```sh
cmake --build build -j2 --target judas_terrain_v2_surface_tests judas_terrain_v2_native_benchmark
build/judas_terrain_v2_surface_tests --write /tmp/terrain-surface-unique
build/judas_terrain_v2_surface_tests --read /tmp/terrain-surface-unique
build/judas_terrain_v2_native_benchmark /tmp/terrain-benchmark-unique
node scripts/check_judasjs_api.mjs
python3 tools/validation/qualify_terrain_v2.py --build build --output /tmp/terrain-export-unique
```

Run geometry/world/component/render/editor controls as in stage4-authoring.
The native test covers rotated owner coordinates, four-chunk publication, old
support during preparation, paint without collider replacement, sparse combined
saves, corrupt overrides, out-of-domain input, actual JS and persistent edit-budget
rejection. Separate write/read processes restore height, paint and excavation.
The relocated gameplay driver uses the new tools, then traverses the tunnel,
saves/quits and restores. Actual rendered evidence is retained.

The first two cold TEST oracles attempted to re-import an existing asset, then
used an asset-directory-relative path. AssetDatabase requires project-relative
paths. Both failed logs remain; production importer was not changed.
The first export correctly rejected mismatched runtime/exporter provenance after
intermediate source edits. Frozen rebuilt products passed the same guard.

No new Windows/Android/browser or human acceptance is claimed. Not the final
Terrain V2 delivery: planetary excavation, residency and integrations remain.

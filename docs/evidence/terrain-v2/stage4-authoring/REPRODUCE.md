# Native authoring qualification

Use a fresh `/tmp` directory for each editor fixture. Run GPU/editor tests on
the desktop NVIDIA backend; portable uses `JUDAS_GL_PROFILE=portable`.

```sh
cmake --build build -j2 --target judas_terrain_v2_creation_tests judas_terrain_v2_generation_tests judas_terrain_v2_editor_tests judas_terrain_v2_authoring_benchmark
build/judas_terrain_v2_generation_tests
build/judas_terrain_v2_creation_tests /tmp/terrain-fresh-unique
build/judas_terrain_v2_editor_tests /tmp/terrain-editor-unique
build/judas_terrain_v2_render_tests
JUDAS_GL_PROFILE=portable build/judas_terrain_v2_render_tests
build/judas_terrain_v2_authoring_benchmark 129
build/judas_terrain_v2_authoring_benchmark 257
build/judas_terrain_v2_authoring_benchmark 513
python3 tools/validation/qualify_terrain_v2.py --build build --output /tmp/terrain-export-unique
```

Geometry/world/LOD/component and separate-process cold tests are unchanged
controls rerun after the authoring changes. Full raw outputs are retained.
The first fresh-project oracle wrongly assumed Project::CreateNew also writes
a scene; the normal editor document supplies it. Two file-picker oracles
used the wrong scoped ImGui item ID; production FileBrowser was unchanged.
Their failed runs are preserved, not counted as engine defects or passes.

No Windows, Android, browser or human acceptance is claimed here. This is
one authoring checkpoint, not complete Terrain V2 qualification.

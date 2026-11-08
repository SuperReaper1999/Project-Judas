# Worker-prepared terrain preview and exact cached picking

Parent checkpoint: `03927f7187161d48b38705e9c3605ca848f42390`. Its engine CI run
42477623102 passed both Alpha and Experimental configurations, and documentation
run 42477623108 passed. No main/release/fluid-lock changes are part of this work.

## Reproduced cost and correction

`TerrainV2Editor::Stroke` previously called the whole merged-mesh picker on every
focused hover, even without a click. `Prepare` merged all vertices/indices on the
editor thread whenever a changed-chunk worker completed. The initial 1 km,
513x513-sample prototype measured 2.498 ms median / 3.620 p95 / 4.585 worst hover
cost over 120 rays and 32.340 ms for the merge. The prototype's chunk-local picker
agreed on its 112 hits, but was not adopted: the final implementation retains the
original globally rounded merged positions and exact original triangle order.

`authoring/TerrainEditorPreview.*` now owns a complete read-only worker snapshot:
shared immutable chunk geometry, the same compatibility mesh and conservative
range bounds. The job captures only immutable source/previous data. The main
editor publishes a complete result, retaining GPU handles for unchanged chunks.
Stale revisions, cancellation and failure retain the previous complete view.
Source switches clear the snapshot and compatibility view together. Readers can
retain an old mesh safely without retaining a chain of old preview snapshots.

`TerrainRayPicking.*` extracts the original two-sided float triangle routine into
one shared implementation. The old four-argument `PickTerrain` API remains.
The fast path rejects irrelevant chunk ranges, then runs that same primitive on
the same flattened indices. The independent frozen original in the tests retains
its own implementation and original-function hash. No physical terrain, collision
accuracy, native terrain data, save format or JudasJS API changes are involved.
Editor preview mesh views are intentionally const; the old read-only test helper
was updated accordingly, with all assertions retained.

## Matched final measurements

Ryzen 5 5600, GNU Release, 524,288 triangles / 1,024 chunks. Three repetitions of
120 identical vertical, oblique and miss rays, alternating call order. No compiler
or GPU test overlapped these measurements. Final reference mesh order matches the
old editor's ordered chunk map. All 112 hit points match the frozen original
exactly in each repetition.

| Repetition | Original median / p95 / worst ms | Cached median / p95 / worst ms |
| --- | --- | --- |
| 0 | 2.43185 / 2.58961 / 3.01947 | 0.005611 / 0.009117 / 0.010640 |
| 1 | 2.42941 / 2.47134 / 2.50555 | 0.005340 / 0.008757 / 0.010921 |
| 2 | 2.42889 / 2.47699 / 2.56298 | 0.005401 / 0.008546 / 0.010419 |

Triangle visits: 62,914,560 → 61,440 per repetition. Bounds: 65,536 bytes.
The original merged vector capacities retain 37,748,736 bytes; exact reservations
retain 22,863,872 bytes plus the bounds. Net measured capacity reduction:
14,819,328 bytes (14.133 MiB). This excludes chunk maps, source/undo data, GPU
resources and allocator overhead. Benchmark RSS includes both reference and
candidate geometry; it is not a production before/after RAM comparison.

Original merge measurements are 29.43–39.45 ms. The complete new preparation
helper, including geometry extraction, merging and bounds, measured 56.07–64.24
ms; it runs on the worker in the editor. These are preparation/query scopes,
not whole-editor frames, a hard owner-thread publication deadline or a ratio
between differently scoped operations. GPU uploads/deallocation can still
contribute to an editor frame. No benchmark-specific coverage reductions exist.

## Qualification

- 2,507 focused checks: independent plane answers, exact frozen primitive and
  cached-range comparisons, seams/corners/parallel and non-normalized rays,
  excavation, full-vs-incremental vertex attributes/index order, actual worker
  ownership, cancellation, failure preservation, source replacement and reader
  lifetime. Actual mapped planetary geometry uses the same builder and oracle.
- Existing native editor controls pass 15 edit/undo checks, 46 creation/palette/
  source-selection/Play controls and 23 planetary editing/Play controls on EACH
  modern and portable NVIDIA path. No assertions or visual checks were removed.
- Original terrain authoring 91, volumetric terrain 69 and world-building 494
  checks pass with the shared primitive. JudasJS contract passes unchanged.
- Validation registration/build organization passes 33 checks with two existing
  environment skips. An earlier invocation guessed a nonexistent Python module;
  its failed log is retained, then the actual `test_cmake_test_targets` module ran.

```sh
cmake -S . -B build
cmake --build build --target judas_terrain_v2_picking_tests judas_terrain_v2_picking_benchmark judas_terrain_v2_editor_tests judas_terrain_v2_creation_tests judas_terrain_v2_planet_editor_tests -j2
build/judas_terrain_v2_picking_tests
build/judas_terrain_v2_picking_benchmark
JUDAS_GL_PROFILE=modern build/judas_terrain_v2_editor_tests /tmp/FRESH-picking-editor
JUDAS_GL_PROFILE=portable build/judas_terrain_v2_creation_tests /tmp/FRESH-picking-creation
JUDAS_GL_PROFILE=modern build/judas_terrain_v2_planet_editor_tests /tmp/FRESH-picking-planet
```

Native GPU calls need desktop access; use fresh directories. Automated tests do
not claim human acceptance. The actual tool still offers the same brushes,
undo/redo, source chooser and save/update workflow. Generic scene selection and
the legacy terrain/volume hover paths retain their original query strategies.
The preview still owns a complete merged mesh; this is not demand paging or
completion of all Terrain V2 work. Main, supported releases, installed RPG SDK,
independent research and the fluid lock remain untouched.

# Native terrain detail transitions

This checkpoint adds a small presentation-owned `TerrainLodTransition` module.
The physical terrain, sampled heights, volume extraction, collision, navigation,
save formats and source/region residency policies are unchanged. Complete LOD
selections cross-fade over 180 ms with complementary screen coverage in the
ordinary colour and shadow programs. Unchanged patches render once; a changed
edge stitch cannot be mistaken for an unchanged patch. No new geometry uploads
or render targets are needed. History contains at most two selections and a
bounded set of shared keys; it is not included in the geometry-payload estimate.

Publication immediately resets history. Old intact heightfields cannot fade
over a newly excavated cave. Zero-time observers select immediately. Other
camera changes are retargeted at a completed fade boundary, avoiding unbounded
history or perpetual restarts. A large teleport can take about 360 ms to settle;
the geometric-error target describes the selected endpoint during that period.
Native mapped planetary patches remain full-detail meshes. This is screen-door
cross-fading, not geometric morphing or temporal antialiasing. A large deliberate
detail jump can show brief stippling, especially at silhouettes.

## Correctness and integration

- Actual NVIDIA modern and portable paths: 98,834 assertions each, mostly pixel
  checks. Intermediate colour/shadow depths exactly match the expected endpoint
  at each sampled screen location; reversing draw order does not change them.
  The endpoints genuinely differ at 32,738 samples. History bounds, retargeting,
  shared patches, changed stitches, reset and invalid time controls also pass.
- Existing GPU height/shadow controls: 978 each; maximum reconstructed height
  difference 7.19726e-6 m, shadow-depth difference zero. Existing planetary cutout
  controls: 343 each. Existing runtime renderer: 54. LOD geometry: 339.
- World: 93, including 300 supported motor steps and zero observed difference
  from the monolithic physical trajectory. Native components: 35. Ordinary
  streaming: 69 write / 13 cold read. Planetary components: 40 / 9; planetary
  streaming: 37 / 14. These are separate processes, not in-process reset tests.
- Native editor planetary workflows: 23 each path; fresh landscape creation: 46.
  Basic Water and source-owned terrain/fluid/particle interaction controls pass.
- Both genuinely relocated consumer games excavate, traverse and cold-load eight
  volume chunks, two surface dabs, ready navigation and an empty edit queue. The
  flat consumer also persists a real reliable particle contact and retires its
  droplet. No existing game or installed SDK selection was changed.
- Shader source pins deliberately advance only mesh/shadow fragments. The
  retained old-pin run reports exactly two failures. After source review and
  native pixel controls, all 47 shader-library checks pass. No other pins,
  tolerances or negative controls were removed.

The initial sharing implementation used C++20 `set::contains`; the C++17 build
correctly rejected it. The final implementation uses `find`; failed and final
build logs are retained. The relocated-game driver also correctly rejected my
first in-checkout output directory. Its guard was kept; the successful runs use
fresh directories outside the checkout. Results were copied here immediately.

## Measured cost, not a speedup claim

Ryzen 5 5600 / GTX 970 4 GiB / NVIDIA 580.178.04, GNU Release. No compiler or other
test job overlapped timing; the desktop remained shared. The kilometre-square,
513x513 sample workload renders two independent views at 1280x800. Each row has
30 samples. GPU elapsed includes submission bubbles. These are colour-render
benchmark scopes, not complete game frames or isolated GPU instruction timings.

| Final measurement | CPU median / p95 / worst ms | GPU elapsed median / p95 / worst ms |
| --- | --- | --- |
| Frozen renderer, final repeat | 0.364 / 0.373 / 0.377 | 0.497 / 0.506 / 0.510 |
| New settled detail | 0.370 / 0.375 / 0.379 | 0.598 / 0.616 / 0.616 |
| Deliberate whole-view transition | 0.456 / 1.042 / 1.043 | 0.956 / 0.971 / 1.011 |
| Ordinary moved-view transition | 0.485 / 0.627 / 0.655 | 0.615 / 0.622 / 0.995 |

Earlier frozen repeats measured 0.515 and 0.579 ms GPU elapsed. There is a small
measurable settled-path overhead, approximately 0.02–0.10 ms in these runs; this
is not claimed to be free. The moved view shares 39 of its old 43 patches with
the new 40-patch selection; shared patches are not duplicated. The two transition
rows describe different camera changes and are not a matched optimization ratio.
No speedup is claimed from different preparation/upload timings or warm caches.

The actual flat and planetary games additionally run 180 intact and 180 excavated
frames through ordinary gameplay and rendering. `actual-game-summary.json` keeps
all median/p95/worst scopes. Excavated medians: flat simulation/scripts/publication
0.375 ms, CPU rendering 0.337 ms, GPU elapsed 0.734 ms; planet 0.893 / 2.321 /
1.236 ms. CPU/GPU scopes overlap and must not be added into an FPS claim. These
are current measurements, not matched before/after improvements. Mapped planetary
patches do not use the new heightfield transition.

## Reproduce and review

```sh
cmake -S . -B build
cmake --build build --target judas_terrain_v2_transition_tests judas_terrain_v2_render_tests judas_terrain_v2_lod_tests judas_shader_library_tests judas_terrain_v2_render_benchmark -j2
JUDAS_GL_PROFILE=modern build/judas_terrain_v2_transition_tests /tmp/FRESH-transition-modern
JUDAS_GL_PROFILE=portable build/judas_terrain_v2_transition_tests /tmp/FRESH-transition-portable
build/judas_terrain_v2_render_benchmark near
python3 tools/validation/qualify_terrain_v2.py --build build --output /tmp/FRESH-transition-flat --particle-region
python3 tools/validation/qualify_terrain_v2.py --build build --output /tmp/FRESH-transition-planet --planet
```

GPU runs require desktop access. Use fresh directories. The visual sequence
deliberately exaggerates the starting detail difference to expose coverage;
normal camera movement uses the ordinary projected-error selection. Actual flat
and planetary tunnel/reload captures are also retained. Automated captures do
not claim human visual acceptance. Native consumers must relink matching preview
headers/libraries; JudasJS and save formats are unchanged. New Windows, Android,
browser and older-Linux distribution qualification is not claimed.

Remote CI for the preceding packaging/SDK checkpoint `9c21b8de` passed engine run
42451149210 and documentation run 42451149211. This does not certify a future
enclosing transition commit; check its own run separately. Main and the fluid
lock remain unchanged. Per-chunk physical interest, source demand paging, broader
materials and large editor preview picking remain open; this is not completion
of the entire Terrain V2 programme.

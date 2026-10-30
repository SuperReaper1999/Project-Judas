# Native 2D Stage 4 — tilemap authoring

Experimental development toward v1.1.0, based on
`051518ff5b9a55ae4b5b5461a7c415ee54bc6924`. Not published or human accepted.
Main, version/channel policy, Advanced Fluids and independent research are unchanged.
GitHub #25, #26 and #28 were closed as fixed on Experimental, explicitly unpublished;
#17 and #27 remain open. Their earlier implementation is retained.

## Implementation and playable project

[Tilemap Workshop](../projects/tilemap_workshop/Tilemap_Workshop.judasproj) contains
an editable platform trail and top-down village, original pixel art, real planar
physics, collectible triggers and modern save/load. WASD/arrows move, Space jumps,
1/2 change scenes, R restarts, F6/F7 save/load. Gameplay remains project JavaScript.
Original deterministic palette generation is `tools/demos/generate_tile_palette.py`;
`judas_tilemap_workshop` generates the project from ordinary public components.

The [authoring guide](judasjs/tilemaps-2d.md) covers import, slicing, painting,
collision, ownership and export. The painter uses existing scene authoring recipes;
there is no new tile runtime, renderer, save system or JavaScript API. Ordinary
sprites carry palette UVs; ordinary static MK2 boxes provide collision. Greedy
rectangle merging is bounded to 16×16-cell chunks and never bridges erased holes.
Stable frame/cell keys retain sprite identities across rebuilds. Invalid drafts,
removed frames, changed image dimensions and manual managed-child edits reject
before publication. Hierarchy duplication remaps recipes; deletion retires them
in the same undo transaction. Prefabs retain compiled objects, not editable grids.

`src/authoring/Tilemap.*` owns validation and transactional compilation.
`src/editor/TilemapEditor.*` owns palette, layer controls, viewport grid/ray strokes
and transient drafts. Existing editor document/history/preferences owners provide
Undo, theme/layout persistence and scene publication. The palette uses the existing
cached typed resource; source files are revalidated deliberately when rebuilding.

## Verification

Linux Release/GCC 15.2 on Ryzen 5 5600, 8 GB RAM. Selected final gates cover:

- `tilemap`: positive/negative grid validation, ID retention, transactional rejection,
  real floor contact, erased-hole fall-through, rotated gravity covariance, prefab
  construction and actual streaming unload/revisit with restored body momentum.
- `tilemap-editor`: ray-driven strokes, fast negative-coordinate drags, rectangles,
  erase, cancel, Undo/Redo, intervening-edit invalidation, Save As/reopen, independently
  paintable duplication, delete/Undo metadata lifecycle.
- `tilemap-render`: actual framebuffer assertions for atlas colors/UVs, foreground
  sorting and erasure revealing the prior layer.
- `tilemap-application`: actual application movement, scene change, trigger collection,
  modern save/load and restored physical endpoint; no cached-status-only claim.
- Separate `judas_tilemap_tests <gate-output>/tilemap --cold`: a fresh process loads
  the saved snapshot, matches the physical endpoint and resumes contact/motion.
- Existing planar physics, sprite animation, editor preferences and MK2 physics;
  JudasJS API/type/inventory consistency is unchanged and passes.
- Actual SDL/ImGui desktop painting, Undo and Play/Stop in dark 100% and light 135%,
  using isolated project/preferences copies. No operator files are modified.
- A production harness tour exercises both scenes, modern saves and relocation.

These are focused behavioural checks, not universal tilemap/platform certification.
Application pixel tests use Mesa llvmpipe; actual editor checks use NVIDIA/X11.
Windows/Android are not qualified by this Linux pass. No new platform backend or
runtime script binding is introduced.

Early failed test runs are retained: a negative-coordinate test counted seven
cells where floor-to-grid arithmetic requires eight; the editor test target lacked
its camera translation unit; direct ImGui mouse injection was overwritten by SDL's
real pointer. The corrected editor test uses actual SDL warp/button delivery and
checks the intended cell independently, rather than only checking tile count.

## Bounded measurements

`judas_tilemap_benchmark <project.judasproj>` retains five fresh/rebuild samples
following one warmup. Physics uses eight actual falling circles, 60 warmup and 120
measured settled steps at 1/60 s, with landing/plane gates. The control uses one
static box per tile over the same exterior occupied region; it has different
internal seams/body identities, so this is not a trajectory-equivalence claim or
a general MK2 optimization. Times are milliseconds, nearest-rank p95.

| Tiles | Merged boxes | Fresh bake median / worst | Rebuild median / worst | Merged physics median / p95 / worst | Per-tile control median / p95 / worst |
|---:|---:|---:|---:|---:|---:|
|128|4|6.818 / 6.988|8.602 / 9.699|0.042 / 0.068 / 0.609|0.083 / 0.088 / 0.089|
|512|4|28.666 / 33.578|38.107 / 41.039|0.045 / 0.062 / 0.068|0.061 / 0.067 / 0.069|
|2048|8|130.954 / 137.804|165.529 / 200.373|0.039 / 0.043 / 0.068|0.135 / 0.140 / 0.146|

This is a bounded editor compiler: committing a full 2,048-cell stroke can visibly
pause the editor for about 0.2 seconds on this host. Continuous dragging stays a
draft until release. No claim of giant-world tile scalability or improved GPU
rendering follows. Host timing noise is visible in the 128-cell worst step.

## Human checklist and limits

1. Open the workshop; Play both scenes, push crates and collect stars. Save/load.
2. Stop; open View → 2D authoring → Tilemap painter. Select a grid and palette tile.
3. Paint, erase and rectangle-fill; Undo/Redo; verify holes affect actual collision.
4. Add a non-solid decoration layer, change order/flips; save/reopen.
5. Duplicate a grid and edit the copy independently. Try both themes/UI scales.
6. Export and run the package away from the source checkout.

Bounds: 2,048 cells/grid, eight layers, coordinates −512…512. Static fixed frames,
unit physical scale, full-cell rectangles, centered pivots. No animated tiles,
custom slopes, one-way tiles, moving grids or runtime cell editing. Runtime edits
to ordinary entities do not rewrite authoring recipes. Palette changes require an
explicit rebuild. These are deliberate initial scope boundaries, not hidden passes.

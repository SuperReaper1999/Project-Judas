# The Packed Museum — M93 asset lab

Open `asset_lab.judasproj` in Judas Editor or run it with the current runtime.
This ordinary project contains 576 detailed sculptures in four districts, four
cooked rigid meshes, sixteen painted cooked textures, cached remote cameras and
independent split views. Its 8 MiB visual budget demonstrates demand residency and exposes
overcommit when the required working set is larger; it is not a claim of total application memory usage.

## Controls

- F5: watch tour / free flight. The initial tour visits all four districts.
- Mouse + WASD, Q/E: look and fly when free flight is active.
- 1–4: visit a district directly.
- V: independent split-screen views.
- C: enable/disable the cached remote camera.
- F2: switch automatic LOD / full-geometry control scenes.
- F6 / F7: save / load; R: reload authored scene.
- Escape: pause, release pointer; resume through the ordinary UI.

The monitors render a different district on a deliberately slower cadence. Both
views contribute to residency between camera updates. The full-detail control
uses the same cooked data with `render.lod-multiplier: 0`; it isolates geometry
selection rather than comparing different texture artwork.

Use the editor profiler/resource diagnostics for actual timings and memory.
The HUD describes settings and controls; it does not invent live counters.
World origin is Earth-scale while sculpture coordinates stay in the engine's
local presentation chart. This is an asset lab, not a planetary traversal demo.

## Recreate / edit

`scripts/m93_lab.py` generates the deterministic OBJ/PNG source art and ordinary
scene/UI/project files; Pillow is authoring-only. Gameplay remains in
`Assets/scripts/lab.js`. Cook source meshes and images with `judas_cook` as described
in [the pipeline reference](../../docs/M93_ASSET_PIPELINE.md). Source art and
relative cook recipes remain available for inspection and recooking. Closure
exports contain runtime data without the source artwork/recipes.

## Human acceptance

1. Watch the tour; inspect texture detail, silhouettes and transitions.
2. Approach a sculpture; use F2 to compare with full detail.
3. Enable split views and inspect both independent viewpoints.
4. Toggle the remote camera; revisit districts and check resources return.
5. Save, move elsewhere, load and verify the real scene/view restores.
6. Pause/resume, reload and use Editor Play/Stop.
7. Select a source asset, cook a new output, assign it and recook it.
8. Export, move the package elsewhere and repeat the checks.

Linux automated evidence is separate from your visual acceptance. Windows and
Android M93 remain unvalidated. See [measurements and limitations](../../docs/evidence/m93/REPORT.md).

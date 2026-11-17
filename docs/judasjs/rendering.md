# Rendering, light selection and shadows

Judas owns drawing, culling and resource lifetime. Scripts author meaning and
change ordinary transforms/materials/lights, not native graphics calls.
See [entities](entities.md#entitylight--entitysetlightsettings),
[cameras](effects-camera.md), [materials](materials.md),
[local multiplayer](local-multiplayer.md) and [planetary terrain](planetary-terrain.md).

## Dynamic lights

`entity.light` / `entity.setLight(patch)` control one authored point or spot.
A spot faces local -Z. Positions/directions use the current local chart, independent
of gravity. The normal presented transform supplies moving light poses.

The renderer conservatively projects each finite light volume into 64-pixel
screen tiles (grid dimensions saturate at 120 per axis). Portable fragments use the shared texture reader. Modern desktop can use
CPU-filled SSBOs or bounded compute packing, preserving stable record/index order.
Capability/policy selection is explicit; no native graphics scripting API is added.
A maximum of **1024 visible lights per view**
and **128 lights per tile** keeps work bounded. Overload selects stable
intensity/distance relevance; profiler overflow counts expose omitted references.
Large ranges and many overlapping lights still cost more than spatially separate
small sources. There is no shadowless-light registration-order five-light cap.

## Shadows

The shared lighting planner supplies three stabilized directional cascades with
16/48/144 m half-extents and local point/spot sources. The default local shadow
budget is four, configurable from zero through eight sources per view. A point
uses six faces and a spot one chart in an aligned six-layer allocation. Importance
and explicit `shadowPriority` select sources; unselected lights remain illuminated.

Every selected dirty chart is drawn before colour consumes it. Exact cache keys
include relevant caster observations, resource revisions, matrices and LOD inputs;
unknown or specialized presentations conservatively redraw. Runtime and Edit
previews use this planner. Cross-view keys cannot borrow another eye's coverage.
The depth pool grows lazily to at most 54 charts, separately budgeted from ordinary
material residency. Modern cubemap-array views and portable depth arrays implement
the same ownership. Finite resolution/coverage and PCF/bias limits remain.

See [lighting budgets/cache semantics](../LUCIFER_V2.md) and
[gameplay exposure](lighting.md); shadow maps are presentation resources, not saved
physical state or universal whole-planet sunlight coverage.

## Repeated geometry

World and editor opaque passes automatically batch compatible rigid meshes with
identical material/texture state. Transform and tint are instance attributes;
mirrored handedness forms separate groups, and nonuniform normals are corrected.
View/layer/frustum rejection occurs before batching. A group flushes at 16,384
instances. Imported rigid meshes and primitives use the same path.

Skinned geometry, hidden-part draws, alpha-mask and alpha-blend material draws
retain the established path. Transparency retains its sorted blend pass. Different
instance material overrides intentionally separate groups. This is instancing,
not a promise that every scene can render in one draw or avoid entity traversal.
`JUDAS_RENDER_BATCHING=0` disables world/editor batching for diagnostics or a
project whose unique material/mesh workload does not benefit. Batching adds CPU
group construction; its gains depend on real repetition.

## Multiple views and terrain

Each view owns its light grid, viewport, optical-path sampling and shadows.
Generated targets retain existing feedback protection and update cadence. Local
player views remain presentation of one authoritative world, not extra simulation.

Planetary LOD uses the **maximum requested screen error across active views** in
one world-owned selection. It retains the existing shared leaf/residency/build
budgets and shared seam morphing. Additional viewpoints can exhaust that budget;
`budgetLimited` remains honest. Render interest never moves physical collision
interest or removes support. No per-camera duplicate planet or physics terrain
is created. Clipped/off-axis cameras use conservative vertical extent selection;
this is not perfect visibility-driven planetary occlusion.

## Lifetime and diagnostics

Light patches live in ordinary runtime entity definitions, including modern
save and streaming snapshots. GPU lists, instance groups and shadow maps never
enter durable data. A scene teardown removes owners and camera targets normally.
Retired pose-only world-state deltas are not supported.

profiler records light selection/upload, opaque instance submission and shadow passes.
Counters include instanced draws/instances, tile overflow, light-grid GPU bytes
and shadow-array payload bytes. GPU timestamps include command-stream gaps;
they are not a hardware utilization meter. Device qualification is stated in [the current matrix](../DEVELOPMENT_READINESS.md).

## Cooked visual assets

Projects may author `.judastex` textures and `.judasmesh` rigid meshes through the
editor or offline cooker. Existing mesh/texture asset IDs, scene transitions,
material controls and saves continue to use the same public APIs. No native
render dispatch or new gameplay binding is required. LOD is selected independently
for each actual view; collision keeps the base geometry. Optional project visual
residency is a soft memory policy, not a guarantee of total GPU memory usage.
See [cooking, LOD, budgets and current limitations](../ASSET_PIPELINE.md).

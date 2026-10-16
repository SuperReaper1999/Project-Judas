# Rendering and light selection (current M92)

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
screen tiles (grid dimensions saturate at 120 per axis). GPU fragments fetch a
bounded nearby list from one RGBA32F texture, shared by legacy and PBR materials.
No compute shader, texture-buffer extension or desktop-only storage buffer is
required for this lighting path. A maximum of **1024 visible lights per view**
and **128 lights per tile** keeps work bounded. Overload selects stable
intensity/distance relevance; profiler overflow counts expose omitted references.
Large ranges and many overlapping lights still cost more than spatially separate
small sources. There is no shadowless-light registration-order five-light cap.

## Shadows

A depth-array layer holds the directional light; up to eight further layers hold
selected spots. The ordinary world path requests **four spots per camera**.
`castsShadow` requests a slot, not a guaranteed reservation. Selection ranks
visible requests by `shadowPriority + max(color)/(1 + distanceSquared)`, where
colour is intensity-scaled; stable source identity breaks ties. Other sources remain
lit. **Point-light cubemap shadows are not implemented.**

Each rendered camera rebuilds its actual directional chart and spot assignments;
secondary and split views cannot sample a different eye's old shadow coverage.
Authored edit previews do not generate these maps and explicitly discard runtime
shadow history before drawing; they cannot inherit Play mode shadows.
Layer storage grows on demand and is retained until renderer shutdown. The
existing directional projection remains a finite local 50 m-wide shadow window;
this is not cascaded sunlight across an entire planet. PCF/bias approximations
remain. Many shadowed views still require repeated world/caster submissions.

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
Legacy pose-only world-state deltas do not retain light patches.

M56 records light selection/upload, opaque instance submission and shadow passes.
Counters include instanced draws/instances, tile overflow, light-grid GPU bytes
and shadow-array payload bytes. GPU timestamps include command-stream gaps;
they are not a hardware utilization meter. Windows/Android M92 validation is
informational and has not been claimed by Linux rendering checks.

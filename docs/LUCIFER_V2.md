# Lighting and shadows

## Architecture and behaviour

- Scene light components remain authoritative. `lighting/LightingWorld` borrows
  presentation snapshots; gameplay exposure uses the same transform resolver with
  physical, rather than remote-interpolated, poses. Games own damage, power,
  batteries, vehicle controls and other behaviour.
- `lighting/ShadowSystem` plans runtime and Edit-preview shadows. **Every selected
  dirty chart is drawn before its consuming colour pass in the same presented
  frame.** There is no age-based deferral for flashlights, characters, vehicles or
  destroyed lights. Brightness changes reuse depth while changing illumination
  immediately. Disabled/destroyed illumination is removed immediately.
- The context-owned `render/ShadowPassCache` retains exact relevant caster
  observations, source identities, light matrices, GPU-resource revisions and
  camera-dependent LOD inputs. It stores fieldwise values, not lossy hashes.
  Relevant changes invalidate immediately. Direct low-level shadow draws retire
  previous keys. A bounded LRU retains other cameras' inactive local allocations.
- Three texel-stabilised directional cascades have half-extents 16/48/144 m.
  Local sources occupy aligned six-layer groups: point lights use six faces and
  spotlights one. Modern desktop uses immutable depth storage and cubemap-array
  texture views when its complete capability/texture-unit requirements are met.
  Portable desktop/GLES/WebGL use six depth-array charts for point shadows.
- `lighting/LightLists` owns reusable modern SSBO/compute transport. Stable CPU
  relevance selection, the 1024-source/128-per-tile limits and observable overflow
  are preserved. GPU packing retains exact CPU record/index ordering. Three
  reusable input/output pairs have a 96 MiB allocation ceiling. Small lists use
  CPU packing; auto chooses compute at 128 submitted entries. This is a measured
  heuristic, not a claim that GPU work is always faster.
- Light-list shaders are statically specialised. Failed compute preparation falls
  back to CPU-filled SSBOs. Failed publication on both paths is reported rather
  than silently omitting lights. Portable contexts retain the texture reader.
- `world.lightExposure` supplies bounded current CPU direct-light estimates for
  gameplay, without a GPU readback. Explicit source/contributor/ray budgets,
  known-visible lower estimates, potential upper estimates, completeness and age
  prevent exhausted work from being reported as darkness. See the exact
  [JudasJS lighting reference](judasjs/lighting.md).

Renderer V2 still owns graphics contexts, resources, materials, submission and
presentation. No competing renderer or game-specific lighting controller was
introduced. The shared std140 transport contains 54 charts in 3,600 bytes.

## Supported budgets and limits

Default local shadow quality is four sources per view; `ShadowBudget` accepts
0–8. Importance and `shadowPriority` select sources before rendering. Additional
lights remain illuminated but unshadowed; this is explicit quality allocation,
**not permission to reuse stale critical maps**. Requested, selected and
unselected counts are observable in profiling. A game requiring more than eight
simultaneously shadowed local sources in one view exceeds this tier.

The lazy depth pool has at most 54 layers: approximately 216 MiB at desktop
1024-square D24 storage using conservative four-byte accounting, or 54 MiB at
512-square on Android/WebGL. Six-layer allocation also reserves unused faces
for spots, trading memory for simple stable source ownership. Cache observations
are limited to 256 KiB per physical layer; oversized keys redraw. The pool is
independent of ordinary material/resource budgets.

Published Runtime Terrain V2 reuses exact depth when the light projection
and relevant draw inputs are unchanged. Each chart compares intersecting
terrain pages with their committed sample or cave-mesh revisions, owner
transform and render state, and the active LOD selection and fade. Tile uploads
and eviction leave other charts reusable. A paint edit may conservatively
invalidate intersecting pages. Terrain-heavy caster sets use compact tokens
interned against exact bytes, within a separately bounded renderer budget.

The renderer keeps the existing directional projection for every frame.
A rotating sun redraws charts whose matrices change. When every directional
projection changed and there are no selected local lights, the renderer skips
building a caster key. A continuously moving caster can similarly make its
own chart ineligible before static keys are serialized. An exact observation
after motion stops re-establishes reuse.

Within a presentation sample, cascades also share cooked streaming bounds
and ready mesh material bindings. Each cascade still evaluates visibility,
requests its visible resources and chooses its LOD. A resource revision
change invalidates the prepared bindings, and pending materials keep their
normal resolution path.

Authored Terrain V2 previews, planets, deformables, networked worlds,
camera-textured casters, active ragdolls and unknown compound presentations
still use conservative redraw. Camera-textured casters retain this policy
when the referenced camera is dormant.

Native area emitters/LTC are deferred. Grouped point/spot sources are available
but are not represented as true area lights. Dedicated water optics and unlit
sprite/particle materials retain their existing shading models. Exposure is a
relative direct-light approximation against published MK2 collision geometry,
not lux, GI, material transmission or a render-pixel prediction. Ambient is
unoccluded. Physical query count is bounded, not arbitrary mesh-query wall time.

## Continuous shadow filtering

Interpolate the comparison results of four adjacent 3x3 kernel placements.
Their shared samples form a 4x4 footprint, with per-axis weights
  [1 - phase, 1, 1, phase], phase = fract(projectedUV * mapSize - 0.5).
The two weight sums are each 3, so total normalization remains 9.
Every tap still uses the actual receiver-plane depth at that texel center.
Raw depth values are not interpolated, which would fabricate occluder depths.
Existing bias, map size/count, directional coverage, lighting and geometry are
unchanged. Sampling work is 16 depth fetches per evaluated light versus 9;
shadow-map memory/resolution are unchanged. No throughput speedup is claimed.


Choose actual light budgets in the editor; use `world.lightExposure` for the
bounded gameplay approximation, not a GPU pixel readback. See
[the exact lighting interface](judasjs/lighting.md) and [runtime appearance](RUNTIME_RENDERING.md).

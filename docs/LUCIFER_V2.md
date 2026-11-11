# Lucifer / Lighting V2

Implementation is in progress on Experimental. Human acceptance and release
promotion have **not** occurred. Advanced Fluids V2 remains locked.

## Ownership and current implementation

- Scene light components remain authoritative; `lighting/LightingWorld` creates
  borrowed presentation snapshots. Games own flashlight, damage and power logic.
- `lighting/ShadowSystem` plans runtime and Edit-view shadow passes. All selected
  dirty shadows are rendered before the consuming colour pass, with no age-based
  delay. This includes moving lights, bodies, articulated visuals and disabled
  illumination.
- The context-owned `ShadowPassCache` retains exact relevant caster observations,
  light matrices, source identities, resource revisions and camera-dependent LOD
  inputs. Unchanged depth can survive frames; brightness changes do not require
  depth regeneration. Direct low-level shadow draws invalidate previous keys.
- Three directional cascades and up to eight selected local sources share a
  bounded depth pool. Each local source occupies an aligned six-face group;
  point lights use all six, spotlights one. Source importance may change without
  unnecessarily changing an existing source's physical allocation.
- Modern desktop uses immutable storage and cubemap-array texture views when
  the complete capability/texture-unit requirements are available. Portable
  desktop, GLES and WebGL use six ordinary depth-array faces. The same physical
  occlusion is represented on both paths.
- The common std140 camera transport now holds 54 shadow charts (3,600 bytes).
  This is an internal renderer transport change, not a JudasJS API break.

The default local-source quality budget remains four; selection is explicit
quality allocation, not deferred updating of stale maps. `ShadowBudget` accepts
zero through eight. At 1024-square desktop resolution the maximum 54-layer pool
occupies approximately 216 MiB (four-byte accounting per depth texel); allocation
is lazy and rounded to cubemap groups on modern desktop. The cache retains at
most 256 KiB of exact content per physical layer; larger observations redraw.

Conservative limitations at this checkpoint: terrain, planets, deformables,
networked worlds, camera-textured casters and unknown compound presentations
redraw rather than risk stale depth. Persistent reuse is currently strongest for
ordinary static/rigid/animated mesh scenes. This is not a claim that those
specialized systems have fine-grained change tracking.

## Checkpoints and evidence

Stage 1: `f3dbe274398fc6b047c8ec8cf0b1a5762e875c40` extracted snapshot ownership
without changing rendering behaviour and introduced the original editable
`projects/lucifer_workshop` nighttime courtyard. Scene validation and script
compile/link passed. The existing shader-library, shadow-reuse and shadow-sampling
gates passed after rebuilding their source identities.

Stage 2 focused native qualification passed on both modern and portable desktop:
`renderer-view-uniforms`, `rendering-architecture`, `renderer-shadow-reuse` and
`shadow-sampling`. Controls include exact native multi-view pixels, persistent
reuse, same-frame moving caster invalidation, brightness-only reuse, disabled
lights, all six point faces, a point-shadow-off pixel negative control, resource
changes, camera-backed alpha guards, LOD-dependent eyes and independent UBO
reflection/readback. These scoped passes do not establish full milestone or
platform qualification.

Diagnostic whole-game captures on Ryzen 5 5600 / GTX 970, NVIDIA 580.178.04,
1280x720, VSync off, uncapped, 3 s warmup and 20 s measured gameplay:

| Configuration | FPS average | frame p50/p95/p99/max ms |
| --- | ---: | --- |
| Frozen C33 baseline, existing spot shadows | 376.89 | 2.596 / 3.066 / 3.207 / 4.468 |
| Stage 2, new shadows, initial snapshot | 204.61 | 4.412 / 6.118 / 6.280 / 7.281 |
| Same new shadows with caching disabled | 188.88 | 4.711 / 6.760 / 7.092 / 7.376 |
| Stage 2, exact fieldwise snapshot | 203.79 | 4.412 / 6.132 / 6.354 / 8.857 |

The old renderer does less shadow work, so the old/new rows are a quality/cost
baseline, **not** an optimisation speedup claim. The snapshot correction reduced
median CPU preparation from 1.114 to 0.079 ms and world submission from 1.948 to
0.923 ms. Presented throughput stayed GPU-limited: increased present waiting
absorbed the CPU saving. CPU and GPU durations must not be added together.
The last trial had one frame above 8.33 ms, none above 16.67 ms, and no discarded
simulation backlog. Repeated matched final qualification remains required.

Raw diagnostic identities, inputs, frames, GPU samples and actual images are in
`.cache/lucifer-v2`; they are local evidence, not SDK contents. The kernel log
was readable and had no new entries during the measured capture window.

## Remaining approved work

Complete Stage 3 light-list capability paths, bounded CPU gameplay exposure
queries, authoring/debug integration and documentation. Then complete Stage 4
regression/CI, matching SDK, relocated exports, representative combined gameplay
and visual/human handover. Do not treat this document as a completed milestone.

## Stage 3 implementation and controlled decisions

Modern desktop now uses a statically specialised SSBO light reader. CPU packing
is retained for small lists; at 128 authored entries the default switches to
stable compute tile packing. `JUDAS_LIGHT_LISTS=texture|cpu|compute|auto` permits
matched measurements. Portable contexts retain the texture reader. GPU packing
preserves the CPU relevance order, the existing 1024-source/128-per-tile limits
and observable overflow. Three reusable input/output pairs have a 96 MiB total
allocation budget. Failed compute preparation falls back to CPU-filled SSBOs;
failure of both resource publications is reported rather than silently hiding
lights. The cubemap-array path is independently capability checked.

Three alternating 20-second captures per policy, using identical frozen inputs
at 1280x720 with uncapped presentation/VSync off, produced these medians:

| Workload | texture FPS / p99 ms | CPU SSBO FPS / p99 ms | compute SSBO FPS / p99 ms |
| --- | --- | --- | --- |
| Night courtyard | 203.89 / 6.348 | 204.18 / 6.359 | 203.75 / 6.352 |
| Dense 256-light scene | 59.12 / 19.003 | 62.21 / 18.199 | 62.50 / 17.890 |

The dense workload still fails the 16.67 ms p99 budget. The retained path gives
roughly 6% end-to-end throughput improvement there without reducing light or
shadow coverage. An earlier shared shader with a runtime branch at every light
fetch was rejected: it slowed both ordinary and dense scenes. Its binary, patch
and measurements are retained as negative evidence in `.cache/lucifer-v2`.
These captures precede the final exact-zero contribution early-outs and final
integration qualification; they are controlled path-selection evidence.

`world.lightExposure(position, options)` provides current CPU-side direct-light
exposure against published MK2 collision geometry. It has explicit source,
contributor and ray-query budgets, known-visible lower bounds, potential upper
bounds and completeness flags. Exhaustion is never represented as darkness or
a complete answer. It uses physical transforms rather than remote presentation
interpolation and requires no render readback. Ambient remains unoccluded;
this is relative direct lighting, not photometric lux, GI or a render-pixel
prediction. Collider-less visual objects cannot occlude this gameplay query.
Ray count is bounded; individual world-query wall time depends on geometry.
See `judasjs/lighting.md` for exact options and result keys.

The workshop uses original geometry and project-owned controls, destruction,
power, save/load and exposure display. Its cruiser and pedestrians are scripted
kinematic assemblies, not a claim of native Vehicle V2 driving simulation.
The exposure HUD samples every quarter second; this game-owned display cadence
does not delay shadow rendering. Native area emitters/LTC are deferred: grouped
point/spot sources remain available without presenting them as a true area-light
implementation. Dedicated water optics and intentionally unlit sprite/particle
materials retain their existing shading models.

An actual parented-light regression demonstrated an old stale-direction defect:
a parent rotated from 90 degrees to identity left the spotlight pointing in the
old direction. Derived cached poses were removed; lighting and exposure now use
the shared current transform resolver. The failing source/output are preserved
as a negative control. Native C++ `StaticLight.position/direction` and unused
historical named shadow-slot constants are intentionally retired. Use the
current world transform and source allocation APIs. Existing JudasJS calls are
preserved; `lightExposure` is additive.

The bounded local-source allocator retains inactive camera allocations by LRU,
without evicting sources selected by the current view. Directional charts snap
to render texels to reduce swimming/redundant updates; relevant caster changes
still invalidate immediately. All these final changes require the Stage 4
qualification recorded below before a completion claim.

Stage 3 focused qualification: eight native gates passed (view uniforms,
rendering architecture, shadow reuse, compute, portable compute rejection,
shadow sampling, exposure and light lists); five corresponding portable GL3.3
lighting/shadow gates also passed. The cache suite passed 46 checks, including
independent camera allocations and directional stabilization. Six stored native
image controls were byte-identical; spotlight comparison differed by at most
one 8-bit channel value. Scene validation and both script modules passed.

A disposable authored variant placed one bulb in the initial aiming direction.
Real mouse input destroyed bulb 200, then ordinary K save completed (73,784
bytes). After process exit, a fresh process loaded with L, logged
`broken:{"200":true}`, and completed exact restoration. No direct test-only
mutation of game damage state was used. Both application runs were free of
resource/script faults. Source project saves were not modified.

The first new PBR-workshop capture reached 377.14 FPS, p50/p95/p99
2.675/2.994/3.208 ms, but included a 259.350 ms presented-frame stall, 257.695 ms
of it inside swap waiting, and 0.126924 s discarded simulation time. There was
no kernel GPU fault in the capture window. This is **not** a clean no-hitch pass;
repeat measurement and final combined-workload qualification remain required.

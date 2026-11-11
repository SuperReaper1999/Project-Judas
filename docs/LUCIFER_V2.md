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

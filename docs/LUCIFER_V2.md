# Lucifer / Lighting V2

Lucifer is implemented on Experimental and awaits operator acceptance. This is
an unsupported Alpha V2 source-build preview, not a published release. Main,
Advanced Fluids V2's Alpha lock and consumer RPG SDK selection are unchanged.
Final candidate/SDK identities and completed platform CI outcomes accompany the
handover; this document does not establish human acceptance.

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
  CPU packing; auto chooses compute at 128 authored entries. This is a measured
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

Terrain, planets, deformables, networked worlds, camera-textured casters and
unknown compound presentations conservatively redraw instead of risking stale
maps. This preserves current rendering, but does not provide fine-grained reuse
for every specialized owner. Camera-textured casters retain this safeguard even
when the referenced camera is dormant. Dense scenes can therefore remain
expensive; this is visible in the stress measurement below.

Native area emitters/LTC are deferred. Grouped point/spot sources are available
but are not represented as true area lights. Dedicated water optics and unlit
sprite/particle materials retain their existing shading models. Exposure is a
relative direct-light approximation against published MK2 collision geometry,
not lux, GI, material transmission or a render-pixel prediction. Ambient is
unoccluded. Physical query count is bounded, not arbitrary mesh-query wall time.

## Correctness and workflow evidence

The clean native run at `dd0474502d16d3ca0def9bfb1072767762630a19` passed all
192 current gates in 233.48 seconds on the actual NVIDIA desktop. It covers
renderer/editor, scripting, Terrain V2, Vehicles V2, deformables, particles,
water/lock policy, navigation, networking, persistence and supported primitives.
The kernel contained no new entries during that clean run.

Focused modern and portable GL3.3 controls also passed. They include:

- Native shadow-on/off pixel negatives; moving occluders redraw in the same frame.
- Persistent unchanged reuse, immediate radiance/disabled-source changes, all six
  point faces, multi-view allocation and source-pool eviction.
- Resource changes, camera-backed textures and view-dependent LOD guards.
- Authored Edit-preview occlusion and immediate redraw after a geometry edit;
  neighbouring preview-region casters are included before shadow planning.
- Independent UBO layout reflection/readback through final slot 53 and exact
  texture/CPU-SSBO/compute-SSBO light records and rendered pixels.
- Exposure occlusion, independent attenuation answers, cone/orientation changes,
  exhausted budgets, invalid/stale inputs and fresh-world restoration.

Six stored native image controls were byte-identical; the spotlight comparison
changed by at most one 8-bit channel value. WebGL2's production-owner proof passed
33 checks using Playwright 1.64.0 / existing Chromium revision 1248, a fixed 32 MiB
Wasm heap and explicit context destruction. Its process-group ceiling was
768 MiB; observed peak was 467.2 MiB with no swap. This is scoped software-WebGL
renderer evidence, not a complete browser-game or Android-device certification.

Actual live QuickJS enumeration matches 524 public symbols, 353 native operations
and 281 callable signatures. The checker covers 26 structured result contracts,
58 examples and eight negative drift controls. The new exposure binding is local
to the existing Rendering module; the central dispatcher was not modified.

The matching clean Linux SDK at that checkpoint verified all 15 tools and 818
files. Eight SDK workflow operations passed, including fresh project creation,
Edit/Play/Stop, import tools, unchanged-package verification and relocated editor
and CLI exports. A separate copy of the **packaged** workshop put one bulb in the
initial aiming direction. Real mouse input shot bulb 200; ordinary save completed.
The export was moved, its source copy renamed, and it ran from `/tmp` with the
exact packaged runtime. A fresh process restored `broken:{"200":true}` with an
exact migration report. No private gameplay state was injected.

The real editor Scene tab, a `bulb` hierarchy search/selection and F5 Play/Stop
were also exercised through native UI input in a disposable project. An earlier
ad-hoc pan automation stayed on the inactive Game tab under Wayland and was not
counted as Scene-view qualification. Direct editor-drawable capture and normal
Scene selection supplied the usable preview evidence instead. Search remains
case-sensitive; `bulb` matches the workshop's authored lamp names.

## Whole-game performance

Reference: Ryzen 5 5600, GTX 970 4 GiB, 8 GiB RAM, Kubuntu Linux,
NVIDIA 580.178.04 / OpenGL 4.6. Captures used the real application loop,
VSync off, uncapped presentation, 3 seconds warmup and 20 seconds measured play.
No compiler or other graphics test ran concurrently; images were inspected after
the controlled batch. All 18 captures completed without script/resource faults,
inactive sampled frames or discarded/clipped simulation time. There were no new
kernel GPU errors in the controlled window.

Three alternating trials per frozen old/new workload, medians of trial metrics:

| Fixed inputs / renderer | FPS | p50 / p95 / p99 ms | worst across trials ms |
| --- | ---: | --- | ---: |
| Original courtyard, C33 baseline | 371.73 | 2.589 / 3.143 / 3.339 | 35.449 |
| Same courtyard, Lucifer | 280.48 | 3.655 / 4.339 / 4.500 | 7.861 |
| Combined terrain/32 actors/5,000 particles, baseline | 434.15 | 1.706 / 5.584 / 6.299 | 18.781 |
| Same combined inputs, Lucifer | 260.83 | 2.832 / 7.218 / 8.272 | 20.717 |
| Current PBR Night Watch, 1920×1080 | 225.23 | 4.541 / 4.842 / 5.000 | 5.912 |

The old renderer does **less shadow work**: it lacks the new point-light shadow
coverage and three-cascade presentation. These rows quantify the quality/cost
change, not an optimisation speedup. The frozen combined view is an elevated
observer benchmark, not the corrected first-person terrain acceptance game.
Actual logs confirm 32 ready/supported actors and 5,000 GPU particles with real
contacts. The scene/images were inspected, not accepted as an empty fast run.

The combined Lucifer p99 range was 6.657–9.480 ms: two trials met 120 FPS at p99,
one did not. It had one isolated frame above 16.67 ms across the three trials,
none above 33.33 ms, and no simulation loss. **A p99 budget pass is not a promise
that every frame remains above 60 FPS.** The aggressive 5 ms combined critical
path goal is not generally met by this combined workload.

Additional identical-final-binary controls:

| Workload | FPS | p50 / p95 / p99 / max ms |
| --- | ---: | --- |
| Current PBR workshop, 1280×720 | 381.15 | 2.690 / 2.941 / 3.098 / 6.481 |
| Same workshop, shadow reuse disabled | 341.12 | 3.020 / 3.298 / 3.494 / 4.998 |
| 1,024 modules / 256 overlapping lights | 62.09 | 16.260 / 17.447 / 17.906 / 18.237 |

The dense scene **fails the 60 FPS p99 target**. It requested 32 local shadows,
selected four and reused no charts. Median CPU shadow planning was 14.067 ms;
GPU main-camera time was 16.136 ms. Those overlap and must not be summed.
This workload remains outside the demonstrated 60 FPS envelope.

For 1080p Night Watch, CPU Play median was 1.459 ms and GPU main camera 4.386 ms;
presented p50 was 4.541 ms. For combined trial 1 those values were
2.652 / 2.974 / 2.837 ms. Again, nested CPU scopes and GPU durations are not added.
Measured gameplay RSS peaks were 149.8 MiB at 1080p, 222.7 MiB combined and
199.1 MiB dense. Shadow allocation counters were 144 / 12 / 216 MiB respectively.
Light-list buffers used 0.375 MiB in Night Watch and 1.688 MiB dense. Whole-device
VRAM (including the desktop) peaked at 525 / 397 / 1,019 MiB in the corresponding
first trials; these are not claimed as per-process VRAM measurements.

A separate full-resolution day/night control and physical walking/mouse/flashlight
control passed without script/resource faults or simulation loss; their p99
was 5.551 ms at 1080p and 3.299 ms at 720p respectively.

## Preserved negative findings and changes

- An initial shared light-fetch shader with a per-fetch runtime branch slowed
  both texture and SSBO paths and was rejected. Separate shader variants restored
  ordinary throughput. Three matched policy trials showed dense throughput
  59.12 FPS texture versus 62.21 CPU SSBO / 62.50 compute SSBO, about 6% net benefit;
  ordinary courtyard throughput remained approximately 204 FPS on all three.
  Those path-selection captures predate final exact-zero contribution skipping.
- Exact fieldwise caster encoding replaced slow text encoding: CPU preparation
  fell from 1.114 to 0.079 ms and submission from 1.948 to 0.923 ms. At that stage
  presented throughput stayed GPU-limited; the CPU saving was not falsely claimed
  as an equivalent frame-rate improvement.
- The real parented-spotlight negative control retained an old direction after
  the parent returned to identity. Derived cached poses were removed and the
  shared resolver now supplies current direction. C++ `StaticLight.position`/
  `direction` and unused historical named shadow slots are intentionally retired;
  use world transforms and source allocation. JudasJS changes are additive.
- The first new PBR diagnostic included one 259.350 ms stall, 257.695 ms in swap
  waiting and 0.126924 seconds lost simulation. Main-camera GPU work remained
  2.045 ms; a large gap followed UI before the outer GPU end marker. Do not call
  that 260 ms of busy lighting work. Image inspection overlapped that diagnostic,
  so it is retained as an uncontrolled negative, not a clean timing pass.
- One native UBO colour-control attempt coincided with NVIDIA Xid 13 in that
  process at 22:08:50 UTC on 2026-11-11. No engine workaround was applied. An
  independent SDL/OpenGL control and five repeated original native controls
  passed, followed by the clean 192-gate run. The isolated fault is unexplained,
  **not claimed repaired**. Earlier artifact-upload infrastructure failures and
  the obsolete nine-chart WebGL test fixture are also preserved.
- Native Windows qualification exposed MSVC's reference interpretation of a
  captured queue's `decltype` in existing MK2 code. An explicit `ArrivalQueue`
  type preserves the algorithm. All 17 executable sections of the native GCC
  object are byte-identical; only profiler source-line strings moved. Focused
  collision/rotation and lighting gates passed. Final MSVC build results must
  come from the accompanying exact-candidate CI receipt, not this Linux evidence.

Raw executable/input identities, complete frame/GPU timelines, operating-system
samples, images and failed attempts are retained under `.cache/lucifer-v2`.
`FINAL_PERFORMANCE.json` and `final-capture-runs.json` enumerate the controlled
trials. Final handover records the exact SDK/CI identities independently of these
historical benchmark executables. No private downloaded assets are used.

## Authoring and human acceptance

Launch from the checkout:

```sh
build/judas_editor projects/lucifer_workshop/Lucifer_Workshop.judasproj
build/judas projects/lucifer_workshop/Lucifer_Workshop.judasproj
```

The local packaged equivalent is `build/linux-sdk/judas_editor` or
`build/linux-sdk/judas`, with project
`build/linux-sdk/projects/lucifer_workshop/Lucifer_Workshop.judasproj`.
For a full work-area editor use `JUDAS_EDITOR_MAXIMIZED=1`. Click the **Scene**
tab for Edit preview; Game is the Play surface.

1. In Scene, search `bulb`, select a lamp and open its Light component. Change
   colour/intensity, range, Point/Spot, cones and shadow priority. Spots face local
   -Z. Confirm preview changes and Undo; save authored changes separately.
2. Play: WASD/mouse, Space jump, F flashlight. Walk around pedestrians, columns
   and the doorway. Check shadows while moving and rotating rapidly.
3. E toggles scripted patrol motion, Q emergency lighting and N day/night. Observe
   the moving headlights/strobes and directional cascades. The original cruiser
   and pedestrians are project-scripted kinematic fixtures, not a claim of native
   vehicle driving controls or skeletal character animation.
4. Aim at a warm hanging bulb and left-click. Its emission and illumination should
   disappear immediately and surrounding shadows should respond. The physical
   exposure HUD samples every quarter second; that display cadence does not delay
   shadows.
5. K saves; wait for completed status, quit the process, reopen and L loads. Check
   broken lamps, time/state and continued movement. R restarts; Esc toggles pointer
   capture. F5 stops Editor Play and returns the authored scene unchanged.

Passing automated checks does not substitute for this human acceptance. Do not
promote Experimental, unlock Advanced Fluids or publish a release automatically.

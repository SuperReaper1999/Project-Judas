# Renderer V2 — Experimental

Development baseline: `d7bee296e86dc3f01e401d69512e0c1522aeb525`.
Main remains at `1094d64515604fe9d03fdab479efc7f880b112a8` pending human acceptance.
This work is the renderer portion of the proposed Alpha v2.0.0; it is not a
release or a Vulkan backend.

## Approved scope

1. Preserve matched baselines; introduce capability negotiation and coherent
   renderer ownership modules with portable correctness controls.
2. Improve submission, typed material batching, GPU uploads and resource lifetime.
3. Modernize shaders and view/pass efficiency without reducing lighting quality.
4. Establish bounded optional compute/storage/indirect infrastructure for future
   effects, while keeping gameplay physics and the Advanced Fluid lock unchanged.
5. Qualify the editable lab, consumer/editor journeys, exports and platform paths.

## Foundation checkpoint

Desktop requests OpenGL 4.6, then 4.5, then portable 3.3. `JUDAS_GL_PROFILE` accepts
exactly `auto`, `portable` or `modern`. Forced modern reports failure rather than
silently selecting portable. Android keeps GLES 3.0; browser keeps WebGL 2 and its
bounded outstanding-frame fence. Context capability reports distinguish hardware
support from the selected policy and validate entry points and limits.

`render/GraphicsCapabilities` owns discovery/policy/diagnostics, and
`render/ShaderLibrary` owns canonical shader sources, profile preparation,
compile/link lifetime and source-labelled errors. The eight extracted canonical
shader strings and existing ES adaptation were independently checked against the
baseline. Modern GLSL 450 initially changes the version header only.

Native foundation checks pass: 40 pure and 52 native capability checks, 47 shader
library checks, 16 new submission controls and 44 existing rendering architecture
checks. The initial architecture invocation reused its old default fixture,
failed script registration and then dereferenced the missing VM. A new disposable
output path passes; that first log remains preserved and is not an engine pass.

The same new benchmark compiled with the baseline's frozen committed headers and
engine/GLAD archives provides the comparison. Its first final-drain attempt
missed the last timestamp because the profiler intentionally excludes current-
frame queries. Advancing an unmeasured drain frame fixes the observer; the failed
89/90-sample run is preserved. Final short baseline rows have all GPU samples.

At 1080p on Ryzen 5 5600 / GTX 970, GCC 15.2 Release, 120 measured frames:

| Workload | Baseline CPU median / p95 ms | Baseline GPU median / p95 ms |
|---|---:|---:|
| Mixed PBR, 2048 objects, 1 view, 8 lights | 2.223 / 2.694 | 3.059 / 3.470 |
| Repeated, 4096 objects, 2 views | 3.677 / 3.779 | 3.762 / 3.927 |
| Ordered sprites, 4096, 1 view | 19.586 / 20.114 | 19.690 / 20.230 |
| General particles, 100000, 2 views | 28.037 / 34.712 | 28.301 / 34.824 |
| Overlapping lights, 256 objects, 128 lights | 0.775 / 1.032 | 21.035 / 21.197 |

GPU intervals include submission bubbles; sprite/particle GPU intervals are not
independent proof of shader cost. The light case has a materially different
CPU/GPU balance. These short runs select investigation targets, not final speedup
claims. Foundation mixed/light captures are pixel-identical to baseline, with
broadly unchanged costs. Raw commands, sources, hardware output, unsuccessful
observations and CSVs remain in `.cache/renderer-v2/`.

These foundation measurements selected targets for the completed work below.
They are retained separately from final matched comparisons; no universal
frame-rate or all-platform claim follows from them.

## Ownership and implementation

The renderer remains one OpenGL renderer. `Renderer` coordinates existing public
calls; the implementation owners under `src/render/` are:

| Owner | Responsibility |
|---|---|
| `GraphicsCapabilities` | Context policy, real entry points and numeric limits; hardware support versus selected profile |
| `ShaderLibrary` | Canonical sources, profile adaptation, named compilation/link errors and shader cleanup |
| `SubmissionQueue` | Typed material/resource compatibility; first-seen opaque groups and consecutive-only sorted-alpha groups |
| `StreamArena` | Context/thread-owned, fenced persistent pages with portable orphan uploads, transactional growth and bounded working-set hints |
| `ViewUniforms` | One explicit 720-byte std140 camera/shadow block, dirty uploads and per-program binding recovery |
| `ParticlePass` | Existing CPU particle presentation, exact depth/index ordering and compact GPU billboard construction |
| `ShadowPassCache` | Completed physical-slot chart reuse within one immutable presentation sample |
| `RenderTargets` | Target lifetime and scoped framebuffer/camera/HDR continuation |
| `ComputeDevice` | Optional bounded storage/kernel owners, validated dispatch, explicit consumer barriers and generation-safe handles |

There is no new physics/particle simulation or fluid solver. Existing CPU
particles upload one 32-byte record rather than six 36-byte expanded vertices.
Particle count, alpha order, fragment colour arithmetic and simulation stay the
same. Already sorted sprites coalesce only consecutive compatible submissions;
a later compatible sprite cannot move across an intervening different sprite.

Persistent pages are sized from actual pass demand, capped at 4 MiB per page,
with three pages. Growth can temporarily retain old and replacement pages until
completion/allocation succeeds. Portable uploads retain driver orphaning. Small
projects do not initialize particle or compute resources until used.

Camera/view/shadow data uses the GL 3.3/GLES3 uniform-buffer baseline. The shared material shader now
runs Legacy lighting only for the draw-uniform Legacy material model. Previously
PBR also ran that unused light loop before evaluating its own BRDF/light loop.
PBR equations, normal maps, light lists, shadow filtering and Legacy arithmetic
order are unchanged. The same correction benefits modern and portable profiles;
there is no multiple-program material dispatcher.

Shadow reuse never crosses a `RenderWorldFrame` sample. Keys include physical
slot, light identity, exact chart and player-caster policy. Camera-dependent mesh
LOD also includes camera/projection/viewport; camera-backed caster textures
conservatively disable reuse. This does not reduce shadow resolution, caster
count or update frequency across frames.

A qualification fixture exposed an existing unfinished-HDR/secondary-target
bug: the parent pass was lost and a later resolve could sample its own output.
Target scopes now suspend and restore the complete parent linear pass, separate
same-size scratch images and protect the suspended chart from bounded-cache
eviction. Lighting/weather/material settings remain caller-owned per view;
those global settings are not newly snapshotted by target scopes.

## Modern extension contracts

`ComputeDevice` is a native renderer extension facility, not a game-side compute
scripting API. It allocates nothing before explicit initialization; selected
portable/GLES3/WebGL2 profiles report unsupported. Initialization verifies the
actual current context and limits, rather than trusting a forged capability
snapshot. Default budgets are 64 MiB, 256 buffers and 64 kernels. Handles carry
owner and generation identities.

Callers declare storage access, bound dispatch sizes and shader-side index
checks. After writes, publish the relevant storage, vertex, indirect or CPU
consumer barrier. Diagnostic readback is explicitly blocking and has no place
in the normal frame loop. Image/texture barriers are publication hooks; image
allocation is outside this buffer API. No Vulkan backend is introduced.

All GPU owners must be shut down while their context is current. Their storage
must not cross threads or contexts. The shared view block and normal draw setup
reassert required bindings after other passes. A linked program replacement must
explicitly reconfigure its block binding; current built-in mesh programs remain
immutable until renderer shutdown.

## Controls and qualification

`JUDAS_GL_PROFILE=auto|portable|modern` selects context policy. For diagnosis,
`JUDAS_PERSISTENT_UPLOADS=0` and `JUDAS_SHADOW_REUSE=0` independently select the established fallback paths.
These controls do not change scene counts or graphics quality.

The [5 November fluid comparison](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/FLUID_RENDER_V2_COMPARISON.md) found unchanged
typical water-scene GPU cost but a reproducible small-scene tail regression
associated with persistent uploads on the GTX 970. The subsequent
[stream retirement correction](RENDERER_V2_UPLOAD_HITCH.md) removes the measured
spikes while retaining persistent uploads. The original comparison remains
before-fix evidence rather than a claim that every workload improves.

The editable project is `projects/renderer_v2_lab/renderer_v2_lab.judasproj`.
Its README describes the six cases, ordinary controls and save/load smoke route.
The starting scene is modest; stress cases require an explicit selection.

New profiler counters report actual view/light/mesh-instance/billboard upload
bytes and calls, material applications, reused shadow passes and arena storage.
The upload counters exclude asset residency, UI, skin palettes and transient
water meshes; they are not a total GPU-bandwidth estimate.

The original matched benchmark's unfinished-main/secondary HDR order triggered
the pass-state defect. Those raw results and images are preserved as diagnostic
failure evidence. Final performance comparisons use the same corrected observer
against both the frozen original engine and candidate, completing secondary
views before the main view as production world presentation does. Native GPU
timestamps can include CPU submission starvation; particle/sprite GPU intervals
must not be described as independent shader-efficiency measurements.

Final matched measurements and coverage are summarized below. The linked
evidence identifies each tested snapshot and binary; no Alpha release or human
acceptance is implied by automated controls.


## Rejected specialization experiment

A multiple-program PBR/Unlit owner, uniform reflection/revision synchronization
and lazy compilation were prototyped. Long runs showed about 23% lower light-case
GPU time. One intermediate HDR/Unlit invocation failed 12 colour checks; its cause
was not established, and diagnostic/repeated cold-process runs passed. No query,
forced completion or settling delay is retained as a fix.

Inspecting the shader exposed a simpler mechanism: the unused Legacy light loop
was unconditional. A draw-uniform guard produced pixel-identical light-case
captures at about 14.8 ms versus 21.2 ms originally and 16.3 ms with specialization,
including the GL3.3 path. We retained that shared-shader correction and removed
the uncommitted multiple-program owner, reflection/revision machinery and variant
path. Their source, binary, tests, failed observation and measurements are archived
in `.cache/renderer-v2/rejected-specialization/` and the preceding qualification
folders. This is a rejected implementation, not a silently retried production fix.


## Final measured results and qualification

The matched comparison used the frozen pre-renderer Experimental checkpoint,
not an old published SDK, against the shared-shader implementation. Twenty ABBA
processes recorded 50,000 measured frames. All GPU samples completed, and all
28 canonical captures were pixel-identical between implementations, including
secondary views. Both binaries use the same benchmark source and workload.

| Workload | CPU median before → after | CPU reduction | GPU median before → after |
|---|---:|---:|---:|
| Mixed PBR, 2,048 objects | 2.177 → 1.748 ms | 19.7% | 2.803 → 1.967 ms |
| Repeated 4,096 objects, two views | 3.538 → 2.012 ms | 43.1% | 3.637 → 2.111 ms |
| Ordered 4,096 sprites | 19.177 → 12.881 ms | 32.8% | 19.223 → 13.017 ms |
| 100,000 particles, two views | 27.913 → 11.153 ms | 60.0% | 28.087 → 11.257 ms |
| 128 overlapping lights | 0.700 → 0.600 ms | 14.2% | 21.215 → 14.886 ms |

The GPU-bound lighting case improves 29.8%; sprite/particle GPU intervals also
include CPU submission bubbles. The 25% CPU / 20% GPU targets are met in the
relevant stressed cases, not every case. No shadows were requested in these
matched performance runs; separate controls qualify shadow correctness/reuse.
Particle instance uploads fall from 43.2 to 6.4 MB per two-view frame without
reducing particles, triangles or alpha order.

See [matched evidence](evidence/renderer-v2/final-performance/README.md) for p95,
worst observations, full commands, hashes, cold timings and RSS. Memory does not
uniformly improve: the first mixed candidate process had an unexplained high
peak, and the repeated case's first completed frame is slightly slower. These
are recorded rather than removed from the comparison. Raw CSVs and rejected
experiments remain in `.cache/renderer-v2/`; compact evidence is in the checkout.

The [qualification report](evidence/renderer-v2/qualification/README.md) records
32/32 final-checkpoint native gates and the exact SDK/export receipts.
Focused native qualification covers the new owners, actual HDR nesting,
shadow/LOD invalidation, sprites, tilemaps, animation, weather, material and asset
workflows, Editor V2 viewports, Scripting V2 registration/lifecycle, UI previews,
physics continuation, origin-shift assemblies, networking and project policies.
Existing M93 asset fixtures are now generated from their original formulas in
each output directory; no historical cache extraction is required. The material
application fixture now copies all required source projects and explicitly
applies its authored advanced-fluid opt-in before testing M55 optics. Alpha
reports that combined historical advanced-runtime fixture unavailable, never a
numerical pass; ordinary rendering and basic-water gates continue separately.
All original optical assertions remain intact.

The new shadow guard has a preserved negative reproduction: the previous
object's per-draw LOD multiplier cannot act as a global policy for another
camera. Resident LOD assets conservatively qualify view-dependent shadow keys,
even when a currently unused asset makes reuse less aggressive.

The editable workshop completes all six scenes, pause, multi-view modes and
save/load. Editor Play/Stop preserves authored source bytes. Fresh SDK checking
covers all 17 tools, project creation, script failure diagnostics, native editor
creation/Play/export, relocated execution and a genuinely separate-process save
restore (saved 37 replaces constructor default 0). The traced relocated run
reads no source-checkout or SDK paths. These are host development packages;
they do not requalify Ubuntu 24.04 distribution compatibility.

[Portability evidence](evidence/renderer-v2/portability/README.md) records 12/12
translation-unit compiler checks per Windows, Android and browser target. The
production owner proof also passes 33 actual Chromium WebGL2 checks with a
fixed 32 MiB Wasm heap and a bounded, terminated browser process. Windows and
Android executables/APKs were not built or run in this pass. The browser proof
is not a complete game, sustained-memory or physical-device qualification.

## Remaining boundaries and human review

- The renderer still uses forward tiled lighting and CPU scene traversal;
  there is no GPU scene-culling rewrite, Vulkan backend or automatic MDI conversion.
  DSA, texture views and MDI capability reporting does not mean those facilities
  are used by every render path.
- Compute/SSBO/indirect support is bounded native infrastructure with a real
  compute-to-indirect-draw test. GPU particle simulation, fluid rendering and
  physical snow are future consumers, not delivered systems.
- Each active persistent arena has three pages capped at 4 MiB each. Instances
  and particles own separate arenas; small consumers allocate less on demand.
  Default compute storage has a separate optional 64 MiB budget.
- Render-target scopes restore framebuffer, viewport, camera, layer and HDR
  continuation, not arbitrary per-view lighting/weather/material settings.
- Normal asynchronous resource publication is frame-boundary driven. Explicit
  blocking resource requests can complete synchronously; shadow reuse remains
  local to one immutable presentation sample, with camera textures guarded.
- Current measurements are one NVIDIA Linux machine, not all-driver performance
  certification. Windows/Android runtime and a full updated browser game remain
  follow-up portability coverage.
- JudasJS APIs, CPU gameplay/physics and release identity are unchanged. Native
  RenderStats schema is version 2, with counters appended to preserve existing
  field order. Advanced Fluids remain locked in Alpha; no release is published.

Human checklist:

1. Open `projects/renderer_v2_lab/renderer_v2_lab.judasproj`, Play, then try cases
   1–6. Confirm materials, shadows, particles and overlapping sprites look correct.
2. Toggle L/H/C/V for lights, spot shadows, camera monitors and split views.
   Stop/Play, save/load and change cases without losing controls or corrupting views.
3. Open an existing project in Editor V2; edit a script, debug/check/reload it,
   then confirm Play/Stop and export remain usable.
4. Compare default desktop rendering with `JUDAS_GL_PROFILE=portable`. Review
   profiler CPU/GPU/upload counters; judge scene appearance rather than expecting
   a universal FPS target.
5. Accept the exact Experimental checkpoint before any main promotion. Platform
   runtime checks not performed here remain pending, not implicitly accepted.

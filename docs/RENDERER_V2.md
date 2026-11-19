# Rendering architecture and graphics policy

Judas has one OpenGL renderer shared by engine, editor and native SDK applications.
Desktop capability selection and GLES/WebGL adaptation are implementation policies,
not separate engines. [Lighting and shadows](LUCIFER_V2.md),
[runtime appearance](RUNTIME_RENDERING.md), [materials](MATERIALS.md),
[cameras](CAMERA_TARGETS.md), [particles](PARTICLES_V2.md) and
[terrain](TERRAIN_V2.md) describe practical feature use.

## Ownership

| Owner | Responsibility |
| --- | --- |
| `EngineHost` / `Window` | Graphics context and context-thread lifetime |
| `Renderer` | Public coordination, handles, passes and statistics |
| `GraphicsCapabilities` | Actual context/entry-point/limit negotiation and selected profile |
| `ShaderLibrary` | Canonical sources, profile adaptation and named shader failures |
| `SubmissionQueue` | Typed compatibility groups and ordered transparent submission |
| `StreamArena` | Context-owned bounded upload pages, completion and portable orphaning |
| `ViewUniforms` | Explicit camera/pass uniform transport and binding recovery |
| `ShadowPassCache` / `ShadowSystem` | Exact relevant caster keys, chart allocation, invalidation and same-frame consumption |
| `RenderTargets` | Target handles and scoped framebuffer/camera/HDR continuation |
| `ComputeDevice` | Optional bounded native storage/kernel dispatch and explicit barriers |
| `ParticlePass` | Presentation of existing particle simulation results |

Resources prepare CPU data on workers and publish GPU objects on the context
thread. Destroying a handle releases its owned resources; leased geometry stays
alive until the final owner retires it. Mutation of immutable shared geometry is
rejected. Per-instance materials, palettes and structural ownership remain distinct.

## Submission and visibility

Opaque geometry groups only when actual resources/material state are compatible.
Sorted-alpha sprites merge only consecutive compatible work; a later compatible
sprite cannot cross an intervening draw. Cameras and shadow passes use their own
frusta. Unknown/invalid bounds fail open; culling never disables physics, scripts,
audio, residency or simulation. UI/debug lines are separate from mesh culling.

Skin matrices and local bounds are shared for identical immutable pose inputs in
one frame. The bounds cache is bounded to 256 owners / 4 MiB and clears at frame
and shutdown boundaries. It is not a persistent approximate-pose cache.

Compatible opaque mesh parts can submit one contiguous shadow draw. Eligibility
requires complete contiguous triangles, identical face-culling/winding and opaque
materials, with safe draw counts. Hidden parts and structural sections retain
per-part submission; masks/blends and incompatible ranges do not coalesce. The
merged path replaces original submissions rather than running underneath them.
Colour geometry, poses, LOD, lights and simulation quality remain unchanged.

## Shadows and view targets

Directional cascades, point/spot allocation, budgets and gameplay exposure are
owned by [the lighting system](LUCIFER_V2.md). Exact shadow reuse can cross frames
when the complete relevant key is unchanged; every selected dirty chart is drawn
before its consuming colour pass. Specialized/unknown presentations conservatively
redraw. Do not apply the older same-sample-only cache restriction to this system.

Target scopes flush pending work before switching views and restore parent
framebuffers, viewport, camera and complete HDR continuation. Lighting/weather/
material policy remains caller-owned. Camera textures retain scene-linear data
for material consumers; display resolve applies exposure/tone mapping once.
See [camera authoring and lifecycle](CAMERA_TARGETS.md).

The editor's Scene/Game presenter controls which primary view advances presentation
cadence. A hidden Game tab does not render merely because its retained target
exists. Auxiliary camera work is explicit; additional views do not step simulation.

## Uploads and native compute

Persistent uploads use three bounded pages, up to 4 MiB per page, sized from actual
pass demand. Replacement growth can temporarily retain both allocations until
completion; failure retains the previous valid storage. Timestamp-query availability
owns upload-page retirement where supported, with bounded polling/deadline handling.
Unsupported timing/persistence capabilities use exact orphan uploads. This is not
a general replacement for compute/readback visibility barriers.

`ComputeDevice` is a C++ extension, not a game-side scripting API. It initializes
only on demand and validates the current context; selected portable/GLES/WebGL
profiles report unsupported. Defaults are 64 MiB, 256 buffers and 64 kernels.
Handles are owner/generation checked. Callers bound dispatch/index access and
publish the actual storage/vertex/indirect/CPU-consumer barrier after writes.
Diagnostic readback blocks and is unsuitable for ordinary frame-loop observation.
The [Linux Vulkan bridge](VULKAN_INTEGRATION.md) lowers supported buffer and
dispatch operations to Vulkan. `ComputeDevice` provides no general
image-allocation API.

## Diagnostic controls

| Setting | Meaning |
| --- | --- |
| `JUDAS_GL_PROFILE=auto|portable|modern` | Select validated capability policy; unsupported forced profiles reject |
| `JUDAS_RENDER_BATCHING=0` | Comparison control for batching |
| `JUDAS_PERSISTENT_UPLOADS=0` | Use portable upload ownership for comparison |
| `JUDAS_SHADOW_REUSE=0` | Draw selected charts without cache reuse |
| `JUDAS_SHADOW_COALESCE=0` | Retain original per-part shadow draws |
| `JUDAS_LIGHT_LISTS=auto|texture|cpu|compute` | Select the supported light-list transport policy |
| `JUDAS_PROFILE_RENDER_DETAIL=1` | With profiling active, aggregate material and GL draw CPU elapsed times |
| `JUDAS_GPU_DIAGNOSTICS=1` | Bounded native debug callback; see [diagnostics](PROFILER.md) |

GL-call elapsed time can include driver waits and is not GPU execution time.
Do not add nested scopes or overlapping CPU/GPU intervals to claim frame cost.
Changing shadows/quality for a test is not a fidelity-preserving optimization.
The [whole-application policy](WHOLE_ENGINE_PERFORMANCE.md) and
[qualification matrix](DEVELOPMENT_READINESS.md) specify measured boundaries.

Portable policy tests on NVIDIA do not certify every GLES/WebGL device. Failed
shader/resource/required capability preparation must surface an actionable error;
feature support and actual device qualification remain separate.

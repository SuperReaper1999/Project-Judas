# Game settings

A standalone Linux game can build its own settings screen with the ordinary
runtime UI. Use project input actions and UI change events to call the settings
API. See [UI](ui.md), [input](input.md), and [audio](audio.md).

`settings.read()` reports the active backend, requested restart backend, current
window size and mode, VSync, FPS cap, global shadows, local shadow chart budget,
and settings errors. Read it when opening the menu or waiting for a display
preview. The listed resolutions are the distinct pixel dimensions SDL reports
for the window's current monitor. `displayModes` lists the available window
modes. Windowed mode also accepts custom sizes.
Borderless fullscreen uses the current desktop mode and refresh rate.

| Setting | Owner and API | When applied |
| --- | --- | --- |
| Window size and borderless fullscreen | Window via `settings.previewDisplay()` | At the frame boundary, with 15 seconds to keep or revert |
| VSync and FPS cap | Presentation and standalone frame loop via `settings.applyVideo()` | Live. Vulkan changes present mode on swapchain recreation |
| Global shadows and local shadow chart budget | Renderer via `settings.applyVideo()` | Live. Direct lighting stays enabled |
| Graphics quality | `settings.applyQuality(presetOrPatch)` | Live at the end of the frame |
| Exposure and weather effects | `world.setAppearance({exposure})` and `weather.quality` | Live. Store the chosen values in game preferences |
| Rendering backend | `settings.switchBackend(backend)` or `settings.applyVideo({backend})` | Explicit live request, or next launch after saving the restart preference |
| Master gain, mute and audio categories | `audio.setGroup("master", patch)` or a project-defined group | Live. Store user choices in game preferences |
| Rebinding and stick dead zones | `input.getBindings()` and `input.setBindings()` | Next input frame. Store the map in game preferences |
| Look sensitivity, invert and gameplay preferences | Game logic, or scale/invert a named input axis | Game-owned |

`settings.applyVideo()` accepts a partial patch. An FPS cap of zero permits
uncapped operation subject to VSync and compositor pacing. Explicit caps range
from 15 to 1000 FPS. Lower caps would leave too little room for the 60 Hz
simulation to catch up. The cap measures
elapsed wall time across the complete frame and waits for the remaining time.
It leaves simulation stepping unchanged. `vsyncRequested` reports the pending or saved choice, while `vsync` reports the
active mode when known. `canDisableVSync` reflects the Vulkan surface capability;
OpenGL reports null because SDL must try the change to establish support. VSync
can be rejected by the current
driver or surface. Vulkan uses FIFO for VSync on and a supported mailbox or
immediate mode for off. `localShadowBudget` ranges from zero to eight and changes
the number of local lights that can receive shadow maps. It addresses the same
setting as the individual quality control. Exclusive display mode switching has
no public control.

For display changes, call `settings.previewDisplay({fullscreen: true})` or
`settings.previewDisplay({fullscreen: false, width: 1280, height: 720})`. Wait
for `displayPending` to clear. A successful preview reports
`displayPreview: true` and a countdown. Call `settings.confirmDisplay()` to keep
it or `settings.revertDisplay()` to restore the previous mode. The engine reverts
an expired preview at a frame boundary. Display errors appear in
`displayError`. Wait for the confirmation boundary before saving.

## Graphics quality

On standalone Linux, `settings.read().quality` lists the supported presets and
individual choices for the active shared renderer. Its `effective` object
contains the applied values. Its `pending` object is the requested configuration
and becomes null once the application attempts it. A matching preset is named
in `preset`. Any other combination is `"custom"`. A different backend can
report different available texture sizes. A session without this contract
reports `quality: null`.

| Preset | Shared shadow size | Sun cascades | Coverage scale | Filter | Mesh LOD bias | Weather | Local shadow sources |
| --- | ---: | ---: | ---: | --- | ---: | --- | ---: |
| Low (`low`) | 256 | 1 | 1 | Fast | 2 | Low | 0 |
| Medium (`medium`) | 512 | 2 | 1 | Fast | 1.5 | Low | 2 |
| High (`high`) | 768 | 3 | 0.85 | Soft | 1 | Authored | 4 |
| Very High (`veryHigh`) | 1024 | 3 | 1 | Soft | 0.75 | High | 6 |
| Ultra (`ultra`) | 1536 | 3 | 1.25 | Soft | 0.5 | High | 8 |

| Preset | Main 3D scale | Heightfield LOD bias | Advanced Water LOD bias | Material anisotropy ceiling |
| --- | ---: | ---: | ---: | ---: |
| Low | 0.50 | 2 | +1 | 1× |
| Medium | 0.67 | 1.5 | +1 | 2× |
| High | 0.85 | 1 | 0 | 4× |
| Very High | 1.00 | 0.75 | 0 | 8× |
| Ultra | 1.00 | 0.5 | −1 | 16× |

| Preset | Decoration density | Decoration distance scale | Planet LOD bias | Optional particle density | Cave detail normals | Water reflection steps |
| --- | ---: | ---: | ---: | ---: | --- | ---: |
| Low | 0.35 | 0.50 | 2.0 | 0.40 | Off | 0 |
| Medium | 0.60 | 0.75 | 1.5 | 0.65 | Off | 4 |
| High | 0.80 | 1.00 | 1.0 | 0.85 | Authored | 7 |
| Very High | 1.00 | 1.00 | 0.75 | 1.00 | Authored | 10 |
| Ultra | 1.00 | 1.50 | 0.50 | 1.00 | Authored | 10 |

- `shadowResolution` remains the legacy control for setting both depths. On
  capable devices, set `directionalShadowResolution` and
  `localShadowResolution` independently to 256, 512, 768, 1024 or 1536.
  The setting is exposed when `quality.independentShadowResolutions` is true
  (at least 18 fragment texture units). The legacy read field reports the local
  value. Presets use shared sizes to retain the same defaults on both device tiers.
  A point shadow needs six local layers. Chart slot padding and on-demand
  allocation mean allocated depth can exceed the currently visible charts.
  `quality.shadowStorageBytes` estimates current depth storage at four bytes
  per texel. It excludes driver overhead. Changing resolution rebuilds the
  affected layout and invalidates cached depths.
- `directionalCascades` chooses one to three charts. Their default orthographic
  half extents are 16, 48 and 144 world units. `shadowDistanceScale` multiplies
  these by 0.5 to 1.5. Actual coverage depends on light and camera direction.
  Local shadow range comes from the authored light. Both controls leave ordinary
  lighting and the gameplay light-query API intact.
- `shadowFilter` chooses a four-comparison bilinear (`fast`) or sixteen-comparison
  soft (`soft`) filter. It changes how shadow edges are sampled.
- `meshLodBias` multiplies authored screen-space error tolerance in colour
  passes. Values run from 0.5 for finer detail to 2 for coarser detail. A mesh
  needs authored or cooked LODs. Materials, collisions, shadow caster selection
  policy, and explicit per-object LOD opt-outs retain their authored behaviour.
- `weather` leaves authored visual quality in place or selects `low` / `high`
  for main views. High increases the cloud target resolution, ray steps and
  visual precipitation budget. Secondary views retain their bounded policy.
  A game can still turn off visual weather with `weather.quality = 0`. Weather
  fields, physical forces and gameplay queries keep their authored behaviour.
- `renderScale` accepts 0.50, 0.67, 0.75, 0.85 or 1.00. It sizes the main
  standalone view's colour and depth work, including lighting, transparency,
  weather and HDR resolve. The engine upscales the resolved colour before
  drawing HUD and text at the full output resolution. Camera rays and pointer
  coordinates keep their native coordinate system. Secondary camera targets
  retain their authored dimensions. Split-screen, pixel-perfect views, and
  editor outputs retain their existing raster dimensions.
- `heightfieldLodBias` multiplies projected-error tolerance for ordinary
  terrain heightfields by 0.5 to 2. Lower values use more detail. All views
  still contribute to the shared visual selection and cave seams stay pinned.
  Cave volume meshes, planet tiles, collision, streaming and saved terrain
  retain their existing policies.
- `waterSurfaceLodBias` selects −1 for finer, 0 for authored, or +1 for
  coarser Advanced Water surface tessellation, within the existing three
  mesh levels. It leaves the water field, optical solver and Basic Water alone.
- `materialAnisotropy` selects 1×, 2×, 4×, 8× or 16×, subject to the
  device's reported sampler limit. Named presets choose the highest supported
  level at or below their ceiling. An explicit unsupported choice fails
  validation. The control affects opaque draws with resident, prefiltered
  mip chains and authored trilinear samplers. It preserves authored wrapping,
  nearest sampling, texture overrides and alpha coverage. The renderer
  reuses sampler state without uploading textures again.

- `decorativeDensity` accepts 0–1. Mark optional boxes, spheres, meshes or sprites
  as **Optional decoration** in their Render inspector. Lower density selects a
  stable subset by scene object ID in colour views and shadow passes. It never
  changes entity visibility flags, physics, interaction, navigation, saves or
  streaming policy. Leave gameplay landmarks and collision cues unmarked.
- `decorativeDistanceScale` accepts 0.25–2 and multiplies the optional **Decorative
  distance** in metres. At zero authored metres a renderable has no distance cap.
  Distance is measured from the main camera to the renderable origin and applies
  to main and local-player runtime colour only. This cutoff is hard. Secondary
  views and shadow culling still use their own visibility policy.
- `planetLodBias` accepts 0.5–2 and multiplies the authored projected-error
  threshold for visual planet tiles. Smaller values refine sooner. Rim pins,
  cutouts, resident limits, morphing, collision and the source's maximum level
  stay authoritative.
- `caveDetailNormals` retains or skips the authored terrain detail normal on
  volumetric cave and planetary replacement meshes. It has an effect when a
  detail normal is assigned. Geometry, seams and collision are unchanged.
- `optionalParticleDensity` accepts 0–1. Mark supplementary emitters as
  **Optional presentation** in the Particle Emitter inspector. The stable
  fraction applies to whole emitters across all render views. An omitted emitter
  continues to simulate, deliver impacts and retain its state on CPU or GPU.
  Use multiple optional emitters to author a graded effect and keep essential
  effects unmarked.
- `waterReflectionSteps` accepts whole numbers from 0 to 10. It limits
  Advanced Water screen-space reflection samples. Zero uses the existing sky
  or environment fallback. Refraction, absorption, underwater composition and
  fluid simulation are unaffected.

Fresh preferences and older preference files retain the previous renderer
defaults. The standalone Linux default is 1024, three cascades, coverage scale
1, Soft filtering, mesh bias 1, authored visual weather and eight local shadow
sources. A saved legacy local budget is retained. `applyQuality("default")`
restores those quality defaults live. It leaves window settings, the global
shadow toggle, and game preferences intact. Call `settings.save()` after the
request completes to persist the new configuration.
Preferences saved before the new controls keep native rendering resolution,
authored heightfield and water detail, and authored texture filtering. They may
then display as Custom. `quality.mainView` reports the most recently rendered
main-view dimensions. Compare it with the output dimensions to detect a native
view or allocation fallback. It is null before the first view.

Use the preset dictionary to populate a menu and the choices to constrain
individual inputs. The game owns presentation. The
[settings example](examples/graphics-quality.js) shows how a HUD event can
request a preset or an individual setting and how a later `update()` can save it.

Call `finishQualityChange` once per request after the pending value clears.
Invalid values throw synchronously. A failed frame-boundary reallocation leaves
the previous settings active and reports `qualityError`. Runtime shadow-array
growth depends on the visible lights and available device memory. If allocation
fails during view preparation, that view keeps direct lighting and skips shadow
sampling. The error is reported and the renderer waits for a quality request
before retrying the rejected allocation size. Finish pending quality changes
before saving or switching backends.
An in-process OpenGL/Vulkan switch carries the effective quality configuration
over, including when the runtime recovers the old backend.

### Other rendering controls

| Area | Current engine boundary |
| --- | --- |
| Cave geometry | Volume chunks and cutout rims use matching topology. Simplifying them would require an independent seam-aware render mesh pipeline. |
| Unmarked world objects | Entity-wide distance and density changes need explicit authoring intent. Required objects and ground collision visuals should remain unmarked. |
| Effects simulation | Quality does not reduce particle or water simulation. Optional particle density operates on whole emitters. |
| Additional reflections and post-processing | The Advanced Water screen-space trace and the existing exposure controls are available. General scene reflections and a post-processing quality ladder require separate render passes. |


## Backend selection and recovery

OpenGL is the default. Backend entries are `active` for an initialized backend,
`candidate` for a compiled backend that still needs to initialize on this host,
or `failed` for a saved Vulkan request that failed during this launch. Only an
active entry proves availability.

When `settings.read().liveBackendSwitch` is true, call
`settings.switchBackend("vulkan")` or `settings.switchBackend("opengl")`.
The call queues work and returns immediately. The current frame finishes, then
a static engine transition screen appears. The native window may briefly
disappear while the replacement context opens. Simulation and script callbacks
pause during reconstruction. The next UI update can inspect
`backendTransition` and `backendError`. A completed switch updates the
effective backend and the in-memory preference. Call `settings.save()` after
completion to use that backend at the next launch.

A failed candidate triggers one attempt to recover the previous backend.
A recovered session reports `"recovered"` and retains the initialization error.
If both devices fail to open or restore, the runtime reports both errors and
exits. A live switch never writes preferences on its own. Finish display
previews, save operations and scene transitions first. Editor Play, debug
sessions with the physics debugger active and network sessions do not support
live switching. `settings.applyVideo({backend})` and `settings.save()` still
provide the restart workflow.

Launch environment overrides take priority over saved preferences.
`JUDAS_GRAPHICS` retains the explicit launch policy including Vulkan failures,
`JUDAS_GL_PROFILE` selects the graphics feature tier, `JUDAS_WINDOW_SIZE` locks
window preferences, and `JUDAS_SHADOWS` locks the global shadow switch. The command
line selects the project and scene and has no competing video switches. Without
an override, the runtime tries a saved Vulkan preference. If startup fails,
it reports the error and tries OpenGL once. The saved preference is preserved so
the user can inspect the failure and choose OpenGL. On Linux, a saved backend
preference can always be overridden with `JUDAS_GRAPHICS=opengl`.

## Persistence and game-owned settings

`settings.readGame()` returns a detached copy of the game-owned JSON object.
`settings.setGame(values)` replaces this in-memory object.
`settings.save()` writes the object and confirmed video settings together by
atomic file replacement. The file lives under
`$XDG_DATA_HOME/Judas/settings/<project identity>/preferences.json`
(default `~/.local/share`). Preferences are shared across character saves and
export relocation preserves the authored project identity. There is no automatic
write on launch or during scene changes.

Missing preferences use defaults. Invalid or newer preferences are left on
disk and the error is reported. `settings.resetSaved()` explicitly replaces the
saved document with defaults for the next launch. It leaves the running session
as-is. Reads return `null` during editor Play and the deterministic capture
harness, which leave the game's preference store alone.

```js
import {settings, audio, input, weather, world} from "judas";

// Call once from the first update (or uiUpdate) of the scene.
// The hidden startup pass has no live input owner.
export function restoreOptions() {
    const preferences = settings.readGame();
    if (!preferences) return;
    if (preferences.master) audio.setGroup("master", preferences.master);
    if (preferences.bindings) {
        try {
            if (!input.setBindings(preferences.bindings)) console.warn("Input is not ready");
        } catch (error) {
            // A newer game build may have changed its authored action map.
            console.warn(String(error));
        }
    }
    if ([0, 1, 2].includes(preferences.weather)) weather.quality = preferences.weather;
    if (Number.isFinite(preferences.exposure)) world.setAppearance({exposure: preferences.exposure});
}

// Connect these handlers to the corresponding controls in a runtime UI document.
export function applyOptions({vsync, fpsCap, shadows, volume, mute, bindings}) {
    const previous = settings.readGame();
    if (!previous) return;
    settings.applyVideo({vsync, fpsCap, shadows});
    audio.setGroup("master", {gain: volume, mute});
    input.setBindings(bindings);
    settings.setGame({...previous, master: {gain: volume, mute}, bindings});
    settings.save();
}
```

Audio groups are defined by the game. Music, effects, dialogue and UI can each
use their own group. Audio opens the platform's default output and has no public
live output-device picker. Apply group preferences again when starting a new
scene, since the scene configures its authored groups when audio begins.

Keyboard, mouse and SDL gamepad controls share the existing binding model. A
game decides which overlaps are conflicts because actions from different
contexts may intentionally share a control. Use `input.setBindings(null)` to
restore project defaults and remove the stored map from game preferences when
saving that reset. Local-player maps belong to their respective player.

The new preference store and display mutation are scoped to standalone Linux.
Android and Web retain platform-owned surfaces and existing controls. Windows
remains outside official Judas support.

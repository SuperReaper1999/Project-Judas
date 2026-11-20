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

- `shadowResolution` sets the square resolution of each depth layer. Sun, spot
  and point charts share the array. A point shadow consumes six layers. For
  budgeting, a layer is approximately four bytes per texel. Changing resolution
  recreates the array and invalidates cached depth. Additional layers are
  allocated on demand. A lower local budget changes source selection immediately.
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

Fresh preferences and older preference files retain the previous renderer
defaults. The standalone Linux default is 1024, three cascades, coverage scale
1, Soft filtering, mesh bias 1, authored visual weather and eight local shadow
sources. A saved legacy local budget is retained. `applyQuality("default")`
restores those quality defaults live. It leaves window settings, the global
shadow toggle, and game preferences intact. Call `settings.save()` after the
request completes to persist the new configuration.

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
| Lighting and shadows | The global toggle and local source budget predate presets. The renderer now exposes shared depth size, directional coverage and filtering. Separate sun/local depth sizes would require separate arrays and sampling paths. Cache invalidation is automatic. |
| Geometry and terrain | Existing cooked mesh LODs support the visual bias above. Terrain has a separate visual LOD for height pages alongside distinct cave and planet paths. There is no single qualified terrain control across those paths. Presets leave terrain residency, collision and authored scene visibility alone. |
| Vegetation and texture filtering | Vegetation can be authored as ordinary scene meshes. There is no general decor-only density owner or game-wide material sampler policy. |
| Particles and fluids | Current emitter budgets and grids can own gameplay state, including impacts and fluid coupling. Presets preserve that state. The water surface keeps its existing view-distance detail policy. |
| Reflections and post-processing | Environment maps and scene exposure are authored controls. The renderer has no general quality ladder for dynamic reflections or post-processing. |
| Render scale | Render targets exist for individual cameras. An independent main-view scale needs a qualified composition path that preserves native-resolution UI, depth copies, camera targets and presentation. Use display settings for window dimensions. |

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

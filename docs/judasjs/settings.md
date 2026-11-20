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
the number of local lights that can receive shadow maps. Directional cascade
count and shadow-map resolution retain their engine defaults. Exclusive display
mode switching and a general graphics preset have no public controls.

For display changes, call `settings.previewDisplay({fullscreen: true})` or
`settings.previewDisplay({fullscreen: false, width: 1280, height: 720})`. Wait
for `displayPending` to clear. A successful preview reports
`displayPreview: true` and a countdown. Call `settings.confirmDisplay()` to keep
it or `settings.revertDisplay()` to restore the previous mode. The engine reverts
an expired preview at a frame boundary. Display errors appear in
`displayError`. Wait for the confirmation boundary before saving.

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

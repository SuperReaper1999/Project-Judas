# Editor V2 qualification and review handoff

4 November 2026. Development began from Experimental
`45fe558388710d46d6f346b6e19ca2be9e76d0e6` (Scripting V2).
Main remains `1094d64515604fe9d03fdab479efc7f880b112a8`.
This is an Experimental editor checkpoint, not an Alpha v2.0.0 release or human acceptance.

## Latest review corrections

The subsequent review is recorded in
[the review-correction receipt](evidence/editor-v2-20261104/review-corrections/README.md).
It supersedes the earlier visual screenshots for the current UI. The timing
sample and initial qualification below retain their original checkpoint scope.

The review adds a shared Judas control layer, true parent-first hierarchy rows,
Scene-to-Hierarchy reveal, readable script/advanced groups, model discovery and
asynchronous texture thumbnails with explicit resource retirement. It does not
create scene children for model bones or script references. An alleged wrong
creation type was not reproduced by the original native menu controls; the
reproduced child-display ordering defect is corrected, and creation feedback now
records the requested kind and actual parent for further reports.

## Four-stage result

1. **Shell and ownership:** official pinned ImGui `v1.91.9b-docking`, one dock
   host and panel registry. Application coordination is separated from project
   commands, Play session, viewport tools, previews, preferences and diagnostics.
2. **Panels and workspace:** actual hierarchy, inspector, asset, settings,
   profiler, streaming, UI and collision workflow implementations have local
   owners. Scene/Scripting/Animation/Audio/UI arrangements, named layouts,
   Light/Dark/custom colours and scaling retain existing functionality.
3. **Workflow and extension:** folders/breadcrumbs/list/grid/search, pinned
   identity/Transform, component groups, Judas Tools and a source-linked Console.
   Console registration needed its own module, one catalogue call and a build
   entry, without an application/frame-loop branch. The frozen before/after
   receipt and actual runtime test are retained, not merely a symbol inventory.
4. **Qualification:** native Linux, real GL images and OS windows, existing
   consumers, debugger/reload, fresh packaged projects and relocated exports.
   Original failures and invalid/environment-limited test attempts are classified
   in the evidence. Current final-source provenance gates are rerun after
   checkpointing; their machine-readable receipt is retained alongside the build.

See [owner map and extension instructions](EDITOR_V2.md#source-ownership).
The original application had 2,301 lines and the original panels source 1,498.
The formatted coordinator is 523 lines; the old panels file is an unused four-line
compatibility marker. Responsibilities were extracted into cohesive owners rather than transferred to
one replacement giant file. Line counts are descriptive, not acceptance tests.
Automation is also split into workspace, assets, terrain/async, simulation and
stabilization fixtures. Runtime engine code does not depend on editor or ImGui.

## Correctness work at the composition boundary

- Foreign SDL tool events no longer resize/close the main window or become its
  gameplay controls. Optional content regions map mouse, local-player pointer
  and IME coordinates to the actual Scene/Game window; ordinary runtime defaults
  remain unchanged.
- Scene and Game use separate display targets. Auxiliary cameras and local-player
  targets render before final Game composition, avoiding prohibited nested target
  allocation. A preserve-content pass draws the existing HUD without clearing the
  scene. Default standalone rendering retains its existing entry point.
- The live Scene is an observer of the same world: it honors its supplied camera,
  has separate weather history, and never advances authored camera cadence,
  physical state or scripting. Scene navigation uses editor-local controls;
  text editing does not route controls into gameplay.
- Right/middle presses focus the actual image without a preliminary left click.
  A release observed while another panel owns focus satisfies the gesture-release
  boundary; held pre-existing presses still cannot resume after focus loss.
  Stop reveals/focuses Scene instead of leaving an empty Game tab in front.
- Finished imports/cooks publish through the authoring workflow even when Asset
  Browser is hidden, only in Edit and only for the originating project.
- Shutdown retires previews, leases, the played world, callbacks, UI backends and
  context before the owned engine host. Invalid-project startup failures preserve
  personal preferences and leave no native/UI context behind.

## Measured Linux checks

| Contract | Observed result before final mechanical formatting |
|---|---|
| Scene/Game rendering, focus and actual two-player composition | 60 embedded checks; 75 native detached checks, all passed |
| Window event/coordinate ownership | 23 passed; original native negative control failed 4/12 |
| Panel registry, layouts and recovery | 21 passed |
| Browser model and actual hidden import publication | 17 passed |
| Real fault/breakpoint Console, source opening, unchanged world/VM/files | 16 passed |
| Failed-startup teardown and unchanged preference bytes | 22 passed |
| Camera math, gesture and editor/game input separation | 67 passed, including original held-press controls |
| Native pointer/resize harness | 51 passed |
| Existing project application / secondary cameras / rendering architecture / multiplayer rendering | 42 / 59 / 44 / 14 passed |
| Scripting V2 registry / module-local acceptance / script authoring / presentation | 20 / 8 / 70 / 44 passed |
| Hosted debugger / debugger core / existing consumer debug-reload journeys | 24 / 49 / 37 passed |
| Existing workspace / picking / hierarchy creation / asset actions / preferences | 48 / 36 / 123 / 29 / 37 passed |
| Build organization / current-doc checker controls / editor architecture controls | 22 / 10 / 5 Python tests passed |

Native application journeys additionally exercised resized picking projection,
unsaved-document cancellation, close guards, repeat camera drags, focus loss and
Play/Stop with byte-identical authored scene restoration. The old specialized
9001/9002 picking journey was initially invoked against the wrong project; that
attempt is **not a valid coverage result**. Its independent geometry-picking
suite passed; no fixture IDs or 14-body expectation were retuned.

The packaged development SDK verified all **17 tools**. `qualify_sdk.py` completed
project creation, script-error negative control, Play/Stop, **32 actual tool
operations**, and two genuinely relocated standalone runs. An additional exported
project saved script JSON and a moved real static collision body, then a fresh
process loaded it: marker `48371` and position `(3,2,1)` were checked by the normal
restore callback, independently visible in harness position rows.

These were host-native development packages. No fresh Ubuntu-baseline build,
clean-machine certification, Windows V2, Android hardware or browser qualification
is claimed by this editor pass.

## Diagnostic cost sample

One sequential baseline/candidate run of the same copied developer project,
1280×800 native X11/XWayland, Release GCC 15.2, Ryzen 5 5600, GTX 970/NVIDIA
580.178.04. No concurrent compiler workload. About 110 Play frames each:

| Diagnostic, median / p95 | Prior editor | V2 |
|---|---:|---:|
| UI build CPU ms | 0.190 / 0.240 | 0.252 / 0.291 |
| UI submission CPU ms | 0.060 / 0.106 | 0.410 / 0.440 |
| Editor UI GPU ms | 0.030 / 0.031 | 0.133 / 0.153 |
| Whole frame interval ms, including present waits | 9.993 / 10.206 | 9.995 / 10.219 |
| Process peak RSS from time -v | 148,896 KiB | 147,504 KiB |

This is an overhead observation, **not a rendering speedup benchmark**: the new
layout has a different content area and performs texture/platform composition.
Scopes overlap; CPU and GPU times must not be added as independent wall time.
World geometry, physics cadence and acceptance conditions were not reduced.
Showing multiple actual views costs additional rendering; it does not create or
advance additional worlds. Raw profiles retain worst frames and scope data.

## Visual and workflow follow-up

The first V2 shell was too close to the previous flat engineering interface when
compared with the operator's concept reference. The subsequent visual pass adds
the indigo/teal/magenta hierarchy, vector icons and heading emphasis through the
existing personal appearance controls. The hierarchy now has a top **Add object**
action, type icons and a clear selected row. Inspector identity prioritizes the
editable actual name and type subtitle, with pinned compact X/Y/Z transforms,
separate component cards and a bottom **Add component** action. Asset grid cards,
the workflow strip and the floating Judas Tools utility expose existing tools;
they do not invent component enable states, a new runtime or an audio sequencer.

Opening a scene now frames its world-transformed player area or useful visible
content instead of blindly choosing its first serialized record. This corrected
the initially empty-looking Hearthworld view, whose first record was a planet
centre. The framing helper has direct positive/negative geometry controls and
does not mutate authored transforms, selection or physical state.

Further composition corrections in this follow-up are:

- A live Scene observer translates its editor camera and focus consistently
  when the authoritative world's origin chart changes, then restores its
  authored document chart when observation ends. It remains an observer of the
  existing world rather than advancing simulation.
- Scene overlay controls own their actual screen rectangles. Drops and camera
  gestures over those controls cannot act on the scene image beneath them.
  The live observer also draws the selected existing debug overlays; it does
  not provide a second debug simulation.
- Audio and UI workflow actions are authoring-only during Play and explain that
  the user must Stop. Their existing tools remain available in Edit mode.

Latest focused Linux results for that follow-up, recorded separately from the
earlier measurement table:

| Contract | Observed result |
|---|---|
| Actual Inspector editing, Undo/quaternion preservation, actions, read-only behavior and narrow script fields | 41 checks passed |
| Asset browsing, fresh Grid default and per-session isolation | 18 passed |
| Geometry picking and initial authored scene framing | 47 passed |
| Registry/workspace/layout behavior, including large-text initial rails | 24 passed |
| Scene/Game composition, observer charts, overlay ownership and input | 87 offscreen / 102 native detached passed |
| Window event/coordinate ownership | 23 passed |
| Hosted debugger / debugger core / consumer debug-reload journeys | 24 / 49 / 37 passed |
| Application lifetime and failure cleanup | 22 passed |

These are bounded automated results. Native captures additionally cover Dark at
0.85/1.0/1.5 and Light at 1.25, plus a selected player in a copy of the operator's
RPG project. Capture projects were copies; the operator's project was preserved.
Captures are agent review evidence, not combined operator acceptance. The narrow
script-field correction now has actual registered metadata, preserved-ID input,
serialized-value/Undo and rendered-glyph bounds checks. Initial rail widths grow
with UI scale; saved layouts remain user-owned. Framing is a size-based camera
hint, not collision-aware camera placement or occlusion avoidance.

Two failed test attempts were classified rather than silently treated as engine
defects or passes. The first read-only Inspector test directly forced an internal
ImGui activation on a disabled widget, bypassing the native admission path; the
replacement uses ordinary keyboard navigation/input and verifies unchanged
authored data/history. A browser-state test still assumed the old List default
after Grid intentionally became the initial view. It was corrected to the
documented Grid policy and strengthened to check per-session independence.
Original attempts and outputs remain retained alongside the corrected runs.

The earlier CPU/GPU/RSS table is a **pre-visual-pass diagnostic sample**. It is not
an exact-final-visual-source performance result, and these new correctness counts
do not replace it with an inferred timing improvement. Final committed source,
editor/SDK binary identities and hashes will be recorded after checkpointing in
`.cache/editor-v2/FINAL_FINGERPRINTS.json`; no earlier dirty-build identity should
be mistaken for that final record. Raw follow-up runs remain under
`.cache/editor-v2/visual-qualified-gates/`, `native-visual-qualification/` and the
related visual review/test folders. A portable subset is retained in
[visual follow-up evidence](evidence/editor-v2-20261104/visual-followup/README.md).

Combined human acceptance is pending. Windows V2 has not been rerun, and no new
Android, browser, mixed-DPI or clean-machine qualification follows from these
Linux checks.

## Reproduce and inspect

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas_editor judas_editor_v2_workspace_tests judas_editor_asset_browser_tests judas_editor_v2_viewport_tests judas_editor_window_routing_tests judas_editor_console_tests judas_editor_application_lifecycle_tests -j2
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy python3 scripts/prealpha_validate.py --build-dir build --test editor-console --test editor-v2-workspaces --test editor-v2-assets --test editor-v2-viewports --test editor-window-routing --test scripting-registry --test scripting-module-acceptance --test editor-script-workflow --output /tmp/judas-editor-v2-gates
SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy build/judas_editor_v2_viewport_tests /tmp/judas-editor-v2-detached --detached
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy build/judas_editor_application_lifecycle_tests
cmake --build build --target judas_linux_sdk -j2
SDL_VIDEODRIVER=x11 XDG_DATA_HOME=/tmp/judas-sdk-v2-data XDG_CONFIG_HOME=/tmp/judas-sdk-v2-config python3 scripts/qualify_sdk.py --sdk build/linux-sdk --work /tmp/judas-sdk-v2-fresh-work
```

Use fresh output paths. Linux SDL `offscreen` cannot validate native resize;
its preserved pointer-test failures are environmental, not passes. Xvfb was
unavailable on this host; native XWayland/NVIDIA supplied genuine desktop checks.
The lifecycle failure-isolation test uses Linux XDG paths and is not advertised
as a Windows gate. A stale embedded source fingerprint is rejected by the gate;
reconfigure/rebuild instead of bypassing that safeguard.

[Retained evidence](evidence/editor-v2-20261104/README.md) includes failures,
commands, profiles, extension hashes and workflow receipts. The working build and
full disposable projects are in `.cache/editor-v2/`. Final binaries/SDK identity
and hashes are recorded in `.cache/editor-v2/FINAL_FINGERPRINTS.json` after the
last source checkpoint, avoiding a self-referential commit hash in this document.

## Remaining scope and one human checklist

- Linux native detachment is qualified on X11/XWayland. Native Wayland retains
  in-window docking; Windows V2 and mixed-monitor/DPI setups remain unqualified.
- Targets currently use **1× logical content resolution**; high-DPI images can be
  softer than native backing-pixel resolution. UI scaling itself is supported.
- Agent-reviewed captures cover Dark 0.85/1/1.5 and Light 1/1.25, not every colour
  combination or monitor. Advanced component editors remain incremental tools,
  not a redesigned animation/audio sequencer.
- No intentional JudasJS API break, conversion tool, second VM, docking plug-in
  framework or new release identity was introduced. Fluid policy remains locked.

For combined Editor/Scripting V2 human review:

1. Open your existing RPG and a fresh project. Create a child/root, rename,
   transform, Undo/Redo, save/reopen; try the player and pause-menu presets.
2. Find/import assets using folders/search/grid; attach/edit a script, inspect
   an error in Console, break/step/inspect/continue, save an edit and reload.
3. Play in Game, navigate the live Scene, type in tools without moving the
   player; Stop must return to unchanged authoring.
4. Dock/split/detach/close/reopen panels and Judas Tools. Save/restart/restore a
   layout, change workspace, test theme/scale, reset without losing named layouts.
5. Export, move the package and play it. Report the backend and monitor setup.

Main promotion and Alpha v2.0.0 publication require separate operator acceptance.

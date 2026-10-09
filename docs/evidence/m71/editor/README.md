# M71 scene-view panning evidence

Candidate source is uncommitted and based on accepted
`fb31f0244c9f41f74c439afd99855e3707937edd`. The combined M71 build and execution
records are coordinated in the parent task; this directory covers the bounded
editor change only. Desktop usability remains Conner's acceptance task.

## Finding and ownership

Panning was **absent** from the ordinary scene-view controller. At the accepted
checkpoint, `EditorApplication::FrameEditMode` starts only a right-button camera
gesture; `EditorCamera::Update` rotates the current camera on right drag and flies
with WASD/QE. `LookAt` frames a selection. There is no middle-button navigation
binding, ordinary scene orbit pivot, wheel zoom or alternate projection mode.
The model-import preview's drag-to-orbit view is a separate asset preview.

This source finding explains the reported inability to shift the scene view by
dragging. It is not a claim that Conner's physical desktop gestures have been
observed or accepted. The candidate ordinary editor autotest feeds SDL events into
the existing scene-view input path and checks the resulting camera state.

## Resulting controls and scale

- **Middle-button drag begun in the viewport:** pan along camera right/up. The
  rendered scene follows the drag direction; camera orientation stays unchanged.
- **Right-button drag:** established look around the camera position. While held,
  WASD/QE fly; Shift retains faster flight. The existing camera is not an orbit
  camera.
- **F / Focus selection:** existing framing remains. Its distance becomes the pan
  reference depth. Loading an existing scene retains its existing 40 m framing;
  the camera's initial reference depth without a scene is 8 m.
- **Release / Escape / window focus loss / text-input ownership:** release the
  viewport gesture and capture. A cancelled held button requires release and a
  new viewport press. Play/Stop also retire the gesture.

For perspective height `H` pixels and focus distance `d` metres, scale is
`2*d*tan(60 degrees/2)/H` metres per pixel. A drag `(dx,dy)` applies
`scale*(-cameraRight*dx + cameraUp*dy)` to the camera. Its forward focus anchor and
the existing 8 m placement plane translate equally. Focal depth controls scale;
the existing 8 m placement distance for new objects is retained. Pan uses pointer
displacement, not elapsed frame time. Zero viewport height is a no-op.

Middle input was unused by scene selection and gizmos. Left selection/gizmo input
excludes camera start; presses begun over panels cannot turn into a viewport drag
when moved outside. An active relative capture remains owned across panels until
release or cancellation. Game input maps do not redefine pan deltas: pan reads
the editor window's SDL relative motion. Right look retains its existing input
axis path. No authored object transform or physics state is used to pan.

Help is visible in **View** and in the startup status hint; the README editor
control table documents middle drag.

## Focused validation commands

The parent task adds `judas_editor_navigation_tests` and configures it before the
single planned affected Release build. This headless test checks projection and
camera/focus displacement against independent screen-space expectations,
depth/height scaling, unchanged orientation, existing right look/fly, press
ownership, focus/cancel rearming, text-field/gizmo exclusion and Play/Stop
cancellation.

```sh
./build/judas_editor_navigation_tests
timeout 120s env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 SDL_AUDIODRIVER=dummy \
  XDG_DATA_HOME=/tmp/judas-m71-editor-data \
  JUDAS_EDITOR_AUTOTEST=docs/evidence/m71/editor/ordinary \
  JUDAS_EDITOR_AUTOTEST_EXPORT=/tmp/judas-m71-kinematic-export \
  JUDAS_EDITOR_AUTOTEST_PAN=1 ./build/judas_editor \
  projects/kinematic_lab/kinematic_lab.judasproj \
  > docs/evidence/m71/editor/editor.log 2>&1
```

The ordinary editor session uses the existing duplicate/undo, rendering,
Play/Stop and save automation. Its opt-in pan hook adds SDL middle press,
movement, release, window focus loss/regain and an active pan before Play.
It checks camera and placement focus displacement against the perspective
formula, no document/gizmo edit, correct capture retirement and no restart after
focus regain. Automated focus tests do not replace compositor/physical mouse
acceptance. Text and gizmo ownership also have focused headless checks; actual
typing and dragging remain in the human checklist.

## Changed scope

`src/editor/EditorCamera.{cpp,h}`, `src/editor/EditorApplication.{cpp,h}`,
`src/editor/EditorPanels.cpp`, `tests/EditorNavigationTests.cpp`, README editor
controls and the parent-owned test target declaration. No importer preview,
gizmo mathematics, projection framework or later milestone feature was added.

## Executed ordinary editor result

After the coordinated affected build and performance measurement finished, the
command above ran once on the frozen generated lab and exited **0**. The ordinary
editor recorded **10 navigation checks, all passing** in [editor.log](editor.log),
duplicate/undo restored the baseline, Play/Stop left the authored scene identical,
and its normal export request wrote `/tmp/judas-m71-kinematic-export` with 5 assets.
No source changed during this run. The saved scene and snapshots are
`ordinary.saved.judas`, `ordinary.edit.png` and `ordinary.play.png`.

The offscreen SDL backend reported **7** `Mouse capture failed: No relative mode
implementation available` messages, and EGL reported **1** software-rendering
selection warning. Synthetic SDL deltas and requested capture ownership were
validated; this backend could not verify actual desktop relative capture. The
snapshots contain rendered editor frames, with the existing autotest profiler
covering much of the view. Neither the snapshots nor requested-capture checks
constitute Conner's visual/input acceptance.

[fingerprints.sha256](fingerprints.sha256) records the executed editor binary,
navigation source/tests, frozen lab scripts/scene/project, log/snapshots/save and
exported binary/project/dependencies/lab script. Root owns the build and focused
headless navigation-test results. Linux offscreen editor execution is the platform
evidence here; physical mouse/compositor behavior, typing/gizmo feel and Windows
editor execution remain untested.

## Final renderer/library follow-up

Root corrected primitive-renderer sampler initialization and relinked the final
application libraries. That changed the rendered/exported executable evidence,
so a narrow affected editor run used distinct receipts after root confirmed the
final source freeze. The original session and package above remain preserved.

```sh
timeout 120s env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 SDL_AUDIODRIVER=dummy \
  XDG_DATA_HOME=/tmp/judas-m71-editor-followup-data \
  JUDAS_EDITOR_AUTOTEST=docs/evidence/m71/editor/followup \
  JUDAS_EDITOR_AUTOTEST_PAN=1 \
  JUDAS_EDITOR_AUTOTEST_EXPORT=/tmp/judas-m71-editor-followup-export \
  ./build/judas_editor projects/kinematic_lab/kinematic_lab.judasproj \
  > docs/evidence/m71/editor/followup.log 2>&1
```

The final follow-up exited **0** with **10/10 navigation checks**, identical
authored state after duplicate/undo and Play/Stop, successful save and the normal
5-asset export. [followup.log](followup.log), `followup.edit.png`,
`followup.play.png`, `followup.saved.judas` and
[followup-fingerprints.sha256](followup-fingerprints.sha256) are the final editor
receipts. Screenshot inspection confirms filled floor/crate geometry in the
visible Play viewport; the existing profiler panel still covers most of the
window, including the edit view. No source, tests or project content changed in
this rerun. The same 7 offscreen-relative-capture messages and 1 EGL warning
persist; physical capture and operator visual/input acceptance remain pending.

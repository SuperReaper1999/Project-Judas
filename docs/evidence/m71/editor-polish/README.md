# M71 editor polish and JudasJS documentation follow-up

Operator-requested follow-up before human M71 testing. Accepted base/unchanged
HEAD is `fb31f0244c9f41f74c439afd99855e3707937edd`. This is an uncommitted candidate;
no operator visual acceptance, Windows run or later milestone is claimed.
This receipt records the pre-wheel/picking polish snapshot. The operator's
subsequent selection/zoom feedback and corrected current candidate are recorded
in [the wheel/picking follow-up](../editor-picking-zoom/README.md). The hashes
below retain their original snapshot identity.

## What changed

- Muted dark/teal chrome and existing licensed DejaVu editor font.
- Dedicated Edit render target/projection inside responsive hierarchy, inspector
  and asset-browser rails. Picking, asset placement and camera gestures share
  the actual scene rectangle. Running Play retains its existing runtime view.
- Resizable dividers, compact toolbar, project/mode status, hierarchy search and
  context actions, asset search/filter tabs, full-path tooltips and local file open.
- Common inspector controls have wrapped labels above full-width values. Widget
  IDs remain stable. Transform rotation now snapshots before its first mutation;
  runtime UI names use the shared text gesture/history path.
- Unsaved scene replacement/close confirmation, safer path prompts, remembered
  per-user layout and offline JS/reference links. Scene settings are disabled
  during Play; help remains accessible. Imports publish independently of which
  browser tab is visible.
- [Editor guide](../../../EDITOR_GUIDE.md), [game-by-hand guide](../../../judasjs/getting-started.md)
  and source-verified corrections to JS lifecycle, saves, sockets, camera, UI,
  streaming, entity-property and safe-handle guidance.

No M71 physics, gameplay, binding-registration or project changes were made by
this polish follow-up. The only shared runtime header addition is the editor's
explicit deferred-close hook; the runtime executable remains exactly unchanged.
No new image asset, dependency, editor framework or runtime API was added.

## Focused execution

Release was configured once for the new targets. `configure.log` retains the
existing dependency notices. `build-first.log` is the affected build;
`build-followup.log`, `build-labels-final.log` and `build-dialog-final.log` are
necessary editor corrections/visual-review increments. All compiler warning
counts are zero. No production or unrelated physics/fluid suite was rerun.

| Scope | Receipt | Result |
| --- | --- | --- |
| Layout bounds, non-overlap, small windows, collapse, invalid dimensions | `workspace.log` | 24 / 0 |
| Actual ImGui pointer/text edits, exact Undo/Redo, IDs and narrow widths | `widgets-final.log` | 15 / 0 |
| Camera pan/cancel/focus/Play ownership | `navigation.log` | 31 / 0 |
| Actual offscreen editor navigation, preferences, dirty guard, resize/picking, Play/Stop, export | `session-reviewed.log` | 18 / 0, exit 0 |
| JudasJS declarations, inventory, source drift, existing live enumeration and examples | `api-check.json` | 318 symbols, 27 exports, 221 operations, 15 callbacks, 36 typed examples, 3 negative controls; pass |

The editor session loaded an authored preference fixture with a 100px asset
panel, proving the previously rejected 80–119px range restores correctly. It
then restores the normal 200px panel, saves preferences, duplicates/undoes,
holds and cancels New-scene confirmation, resizes to 960×640 and back to
1280×800, performs ordinary Play/Stop, holds/cancels native close and saves.
The authored scene remains identical. Images `reviewed.edit.png`,
`reviewed.resized.png`, `reviewed.unsaved.png`, `reviewed.play.png` and
`reviewed.restored.png` were inspected directly.

Representative reproduction commands (not additional runs):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas_editor judas_editor_workspace_tests \
  judas_editor_widget_tests judas_editor_navigation_tests judas judas_export -j2
./build/judas_editor_workspace_tests
./build/judas_editor_widget_tests
./build/judas_editor_navigation_tests
node scripts/check_judasjs_api.mjs \
  --typescript /tmp/judas-win-linux-ts/node_modules/typescript/lib/typescript.js \
  --runtime docs/evidence/m71/api/live-vm/surface.json
```

The editor invocation uses `SDL_VIDEODRIVER=offscreen`,
`LIBGL_ALWAYS_SOFTWARE=1`, `SDL_AUDIODRIVER=dummy`, `MESA_DEBUG=context`,
`XDG_DATA_HOME=/tmp/judas-m71-polish-data`,
`JUDAS_EDITOR_AUTOTEST=docs/evidence/m71/editor-polish/reviewed`,
`JUDAS_EDITOR_AUTOTEST_PAN=1`, `JUDAS_EDITOR_AUTOTEST_POLISH=1`,
`JUDAS_EDITOR_AUTOTEST_PREFS=1` and
`JUDAS_EDITOR_AUTOTEST_EXPORT=/tmp/judas-m71-polish-reviewed-export`,
running `build/judas_editor projects/kinematic_lab/kinematic_lab.judasproj`.
The 100px preference fixture is specific test input, not a production default.

## Failures, evidence identity and unchanged runtime

`session-first.log` preserves two failed guard assertions. The test queued
Escape in the same frame the guard opened, so it immediately cancelled the
dialog before checking pending state. The corrected timing holds the actual
popup across frames, verifies visibility, then cancels. No dirty-guard runtime
change was needed for that failure. `session-final.log` retains the intermediate
passing session before adding the visible-popup hold/screenshot proof.
The original 11-check widget result is retained in `widgets-first.log`.

Source review and screenshots also found clipped inspector labels, rejected
valid short panel preferences, ignored help requests in Play and an invisible
Scene-setting toggle in Play. These were corrected in their generic editor paths.

The original `../final-source.sha256` and `../final-verification.json` remain the
pre-polish snapshot. They were not silently regenerated. The current combined
candidate is identified by [final-source.sha256](final-source.sha256),
[changed-files.txt](changed-files.txt) and [verification.json](verification.json).
The latter lists deliberate differences from that snapshot and verifies that
all prior core physics/runtime/project fingerprints remain unchanged.
`api-source-hashes.json` ties the documentation check to actual bindings/types.

The new editor export at `/tmp/judas-m71-polish-reviewed-export` matches all
23 file hashes in the original final package receipt. Runtime SHA-256 remains
`ae42790e00cf28b1aed4f5dd9b07153c623fc6dfd85a8c045359bf33fb42adf8`.
Thus the original moved/source-hidden package checks remain applicable;
unchanged standalone behaviour was not ceremonially rerun. Packages/build
outputs/preferences remain outside Git. See `verification.json` for comparison.

## Human review / limits

The software backend reports one EGL warning and lacks real relative mouse
capture. Logical capture/gesture ownership and actual rendering passed;
physical desktop pan/look feel, click/file-handler behaviour and Windows editor
validation remain for the operator. There is no new orbit/zoom controller,
docking framework, embedded script editor or hot reload. Specialized inspector
widgets can still commit each change rather than grouping a whole gesture.

Use the short [editor checklist](../../../EDITOR_GUIDE.md#review-this-candidate)
alongside the existing [M71 lab checklist](../../../M71.md). Start with resizing,
pan/focus, transform Undo, unsaved Cancel and script attachment/Play/Stop.

# Editor V2 review corrections — 4 November 2026

Experimental only. Main remains `1094d64515604fe9d03fdab479efc7f880b112a8`.
Visual acceptance belongs to the operator. No Alpha release or platform-wide
certification is inferred from these controls.

## Corrected mechanisms

- Hierarchy now traverses actual scene parents depth-first, preserving authored
  sibling order without rewriting scene serialization. The original frozen
  `418b03a0` build passes the creation-kind/parent controls but fails two visual
  adjacency checks. `baseline-tree.log` and `BASELINE_PROVENANCE.json` retain
  that reproduction. Earlier mixed-header/library and incorrect clip-rectangle
  attempts under `.cache/editor-v2-review/baseline/` are invalid fixture attempts,
  not engine failures. The reported wrong “Game menu” kind was not reproduced.
- Scene picks reveal the correct Hierarchy tab/row, expand ancestors, clear an
  obstructing filter and scroll once without stealing Scene keyboard focus.
  Internal context selection does not jump the list under its menu. Disclosure
  double-clicks do not rename/select the parent; keyboard activation remains
  independent of the pointer's position over its arrow.
- `EditorDesign` supplies shared custom-painted actions, toggles, rows, tree
  rows, disclosures, search, fields and surfaces. ImGui retains native identity,
  input, focus, docking and disabled-state ownership. EditorWidgets retains
  document transactions. This editor-only library does not enter the runtime.
- Inspector prioritizes identity and Transform. Script settings, filtering and
  other advanced properties are progressively disclosed. Play permits read-only
  disclosure navigation while authored fields, Add and Remove remain locked.
- Inspect model opens the real source asset and imported parent-first skeleton.
  It is an Edit action, with a Stop hint during Play. Model joints are not
  separate scene children. Existing runtime socket attachments and script entity
  references keep their actual ownership; no fictitious child tree is authored.
- Asset thumbnails reuse asynchronous ResourceManager textures/sprite-sheet
  dependencies. Demand is retired on hidden/details/list/Play/project changes
  and before teardown. Current-frame image commands are replaced before releasing
  resources during a project/content switch. Texture dimensions use cached
  renderer metadata, with no readback or second texture cache.

Thumbnail limits are two pending requests, two starts/frame, sixteen held
entries and a ready-pin budget of min(32 MiB, one quarter of resource budget).
Full resource textures are requested: this is not a strict peak decode-memory
bound or a new downsampled thumbnail format.

## Controls and preserved failures

`first-design-controls.log`, `first-combined-results.json` and the two
`first-*-controls.log`/selection logs preserve unsuccessful checks. Native ImGui
comparisons established that programmatic hidden navigation focus cannot Space
activate even an unchanged button. The final control uses actual Tab navigation,
asserts visible focus and tests activation on both native and custom buttons.
The test also checks frozen-header clipping, complete caption glyphs, disabled
controls, popup closure, text editing and balanced scopes.

Inspector fixtures initially used window-position-relative navigation rectangles.
Pinned ImGui uses its official `WindowRectRelToAbs` conversion. The corrected
fixture enters/leaves child focus through normal Enter/Escape. Its second direct
Edit-to-Play mode change can retain a now-disabled navigation item after a popup;
ordinary backward Tab recovers focus. The real application additionally hands
focus to Game during Play. No native navigation IDs are forced to satisfy these
checks. Original trace runs remain in `.cache/editor-v2-review/inspector-focus-*`.

The previous Experimental CI failure was the registration test's stale tooling
expectation, not failing native engine cases. The fixture now includes both real
Editor/Scripting architecture gates, and a negative control detects their
removal. Registration controls pass 11 tests, with two unavailable-generator
cases skipped. Architecture/build controls pass 27 tests.

## Linux qualification results

- All fourteen selected editor/scripting gates pass. Shared design controls
  **33**, Hierarchy reveal/navigation **28**, actual creation menus **37**,
  thumbnails **37**, Inspector **65**, existing hierarchy/creation **125**.
- Workspaces **24**, Console **16**, embedded viewports **87**, native detached
  viewports **102**, window routing **23**, preferences **37**, widget lifecycle
  **15**, application teardown **22** all pass.
- Scripting registry **20**, module acceptance **8**, authoring **70**,
  presentation **44**, hosted debugger **24**, debugger core **49** and existing
  consumer debug/reload journeys **37** all pass.
- The host-native development SDK verifies seventeen tools and thirty-two real
  tool operations. Project creation, Play/Stop and two genuinely moved standalone
  exports pass. Separate relocated-process save/write and restore pass, including
  marker `48371` and independently logged physical position `(3,2,1)`.
  This is not a fresh Ubuntu-baseline or clean-machine qualification.

An extra, non-gate historical `judas_developer_integration_tests` invocation was
mistakenly selected while locating the hosted debugger target. It reports
**62 checks / 1 failure** for “new gravity-driven TOI wakes sleeper with an
anchored motion ledger.” The unchanged test conjoins waking, finite position and
`impactEvents > 0`. Its isolated reproduction observes sleeping **1 → 0**, finite
crate Y **0.498296887**, sphere Y **1.20029688**, but **zero** impact events. Physics
sources have no difference from main. This counter-based historical expectation
is retained as a separate open test-contract diagnostic, not a green result or
an introduced physics repair. Neither fixture nor solver was changed to hide it.
The original log and minimal observation source/output are retained here.

## Visual evidence

Real maximized X11/XWayland client captures are **1920 × 998**, excluding desktop
panels and window decoration, on GTX 970 / NVIDIA 580.178.04. The actual copied
RPG project contains 597 objects. Dark 100%, Light 125% and Dark 150% captures and
logs are retained here. All three exit normally, show asynchronous ready images
and preserve the authored scene through the duplicate/Undo/save controls.
These idle-browser journeys deliberately remain in Edit; they are not Play
qualification. Separate normal Play/Stop and SDK journeys provide that evidence.

The concept image is a design target, not an implemented game world or screenshot.
These captures contain the project's real sandbox, not replacement concept art.
Advanced editors remain incremental tools. Native Wayland detachment, Windows V2,
Android V2, browser V2 and mixed-monitor DPI were not qualified by this pass.

## Reproduction and final identity

Run the commands in `COMMANDS.txt`. The machine-readable final gate receipts and
SDK workflow receipt are retained beside this file. Full disposable projects,
logs and binary/source fingerprints remain in `.cache/editor-v2-review/`;
`FINAL_FINGERPRINTS.json` there records the committed candidate and exact editor /
SDK tool hashes after checkpointing. It does not replace the older 418b receipt.

## Human review

1. Select in Scene, verify immediate Hierarchy reveal; fold, rename and create a
   root/child shape. Check parent, Transform, Undo/Redo and save/reopen.
2. Inspect the player model, browse real asset previews, open/attach/edit scripts
   and expand Settings. Confirm advanced controls remain accessible.
3. Play/Stop; inspect read-only sections; break/step/continue/reload with Scripting
   V2 and confirm game input stays with the appropriate view.
4. Dock/float/resize panels and Judas Tools; restore named layouts. Judge the
   visual hierarchy in both themes and at your preferred scale.

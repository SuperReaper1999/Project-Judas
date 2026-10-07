# M67 — integrated world building (provisional)

Starting checkpoint: `b9a8cf3b8d8cd3786272480a8abe6cbe9fffe859`.
This document records current work; historical consumer reports remain unchanged.
The operator authorized a provisional checkpoint on 7 October 2026. Proper human
authoring, visual and interactive validation remains deferred and unaccepted.

## Starting coverage

| Requirement / consumer evidence | Starting coverage | Shared implementation to extend |
|---|---|---|
| Selection, mixed properties, copy/paste, references (Skate C-02/C-09) | Partial: transactions exist; parent/child transforms and cancellation need correction | `editor/EditorDocument.h`, `EditorHistory.cpp`, `Prefab.cpp` |
| Named project/scene/UI authoring (Skate C-06, Rooftop E7) | Partial: one-way JSON converter, scene property validation already shared | `tools/StructuredAuthor.cpp`, `SceneSerialization.cpp`, `Project.cpp`, `RuntimeUI.cpp` |
| Arrays, paths, curved physical geometry (Skate C-01/J13) | Missing | Existing scene identities, jobs and M64 collision cooker |
| Multipart models and reimport (Skate C-04/J05/J06) | Existing and adequate | M66 model import/archive/material paths; keep stable subassets and last-good products |
| Skeleton/socket/IK picking (Skate C-03/J12) | Partial: pickers present; generic fitting and batch mapping missing | M65 editor pickers, M48 mappings, M66 skeleton data |
| Source-located physical diagnostics | Existing and adequate | M64 cooker and M66 source-selection diagnostics |
| Camera range/projection (Void S2) | Partial: secondary cameras author near/far; script view hardcodes 0.1–500 m | `WorldPresentation.cpp`, `WorldCharacter.cpp` |
| Runtime UI placement (Void S6) | Missing public layout mutation | Existing `RuntimeUI` layout/input/draw path |
| Prefab construction data (Void S8) | Missing: transform only | Existing transactional `RuntimeWorld::SpawnPrefab` and script-slot lifecycle |
| Export selection, screenshots, harness, reference safety | Existing and adequate | Accepted M65 repairs; no replacement |
| Streaming/save/load/localized text | Existing and adequate; integrate authoring | M59/M61/M58 services; preserve their identity and compatibility contracts |

The source of truth is current code. Evidence supplied in the M67 archive documents
historical pain; it does not override accepted M65/M66 behaviour.

## Candidate architecture and closure

M65 commands/selection/property infrastructure and M66 multipart import, stable
subassets, reimport/material overrides and cooker diagnostics are reused. New
shared `judas_world_authoring` services provide recipe evaluation/publication,
source documents, typed dependencies and skeleton fitting. Normal Project, Scene,
Prefab, UI, InputMap and WorldManifest validators remain authoritative.

- Skate C-01/J13: linear/radial arrays, sampled/smooth paths with transported frames,
  closed convex profile sweep/arc, editable render/proxy segmentation and actual
  M64 collision cooking. No concave CAD/CSG or artist-topology repair.
- C-02/C-09: hierarchy filtering/ancestor context, range/toggle selection,
  parent/child-once world edits, local/individual/shared pivots, mixed edited fields,
  alignment/snaps/rename/folders, explicit parent policy, compatible component
  batches and remapped subtree duplication/deletion/reference review.
- C-03/J12: actual skeleton pickers and axes/socket/limb target inspectors;
  selected-chain rest-shape fitting previews plus explicit batch limits/resistance.
  Existing mass/limits/resistance and unmapped joints are retained.
- C-04/J05/J06: established M66 import/remap/reimport remains intact; browser adds
  load/missing status, typed dependency/user discovery. No VM per selection/query.
- C-06/Rooftop E7: [schema-1 named documents](NAMED_AUTHORING.md) are direct normal
  loading/saving files, including current project/UI/input/world fields. CLI and
  source widgets use the same normalized validators; old one-way M65 converter
  remains labeled compatibility documentation.
- Void S2/S6/S8: configured main/secondary camera range and viewport projection;
  safe batched transient UI layout; independent bounded prefab properties/state
  and initial world motion. Types, reference, inventory and executed public example
  accompany each addition. These are primitives, with game meaning in JS.
- Void specialist offline asset work is not a generic DCC replacement. Existing
  resource publication/streaming cold costs are measured separately; no later
  whole-world optimization or gameplay feature work is introduced.

## Editing, identity and lifecycle

Scene history retains 200 complete authored revisions. One accepted command or
completed drag creates one revision; cancellation restores its baseline. Selection
is not an edit. Parent/child selection transforms roots once. Folder labels do not
parent entities. Preserve-world parenting rejects cycles/unrepresentable shear.
Generic property batches reject identity/parent fields and direct the user to their
normal dedicated commands; they cannot silently discard these edits.

Recipe element keys/IDs survive regeneration. Fields matching their last generated
value update; edited values remain overrides. Removal is listed and requires
acceptance; remaining normal references must validate. Detach retains ordinary
objects/assets. New geometry revisions derive mesh/proxy/collision IDs from recipe identity and
content digest; existing accepted revision IDs are retained, preventing
undo from pointing at overwritten physical data. Exact source/document/project
revision checks block stale job publication. Cancellation is cooperative and may
finish bounded cooking before acknowledging; workers do CPU work only.

Runtime Play copies normal authored content. Stop, project changes and job staleness
cannot write runtime transforms back into source. Editor preference/layout state
remains separate from gameplay identity. Supported physics keeps explicit body
sizes: visual transform scale is not automatic rigid-body rescaling. Surface snap
places **origins** with a real query; optional authored local +Y-to-normal alignment
is placement policy, not a universal gravity direction or shape-bottom fit.

Named source drafts, validated documents, dirty flags and errors are separate.
External changes block overwrite and provide explicit reload/diff choices. Invalid
batches/drafts keep last-good content. Per-file staged publication has rollback and
journals; [exact guarantees and limits](NAMED_AUTHORING.md#publication-and-recovery)
do not claim filesystem-wide/crash atomicity. Project changes retain source drafts
for the previous scene in this editor process; not a disk autosave service.

## UI and runtime seams

Assets → UI → Edit UI document opens a resizable properties/source and canvas
window with hierarchy/canvas picking, layout/style,
parent-driven effective rectangles, source/visual undo, subtree duplication and
batch style/copy-paste. Preview sizes/locales use the **same RuntimeUI and Renderer**
shaping/layout/clipping path as Play, including M58 RTL/fallback/wrapping.

`world.setView(pose,fov,{near,far})`, `world.project(point)`, `world.viewport`,
`entity.setCameraProjection(range)`/`entity.camera`, `UIElement.layout` and
`setLayout(patch)` are documented presentation primitives. Layout writes invalidate
one revision and drawing/hit/focus use the same cached geometry; they are transient.
Projection uses actual aspect/near/far, top-left normalized coordinates and reports
behind/depth/inside. Larger far/smaller near ranges reduce depth precision; fixed
origin representation is unchanged, not live origin rebasing.

`world.spawnPrefab(asset,pose,options)` preflights hierarchy/resources/script slots/
properties/state before publication; body velocities are initialized before first
integration. Script construction/start follows existing synchronization/readiness
and state is offered once as `context.initialState`. M61 cold restore uses saved
state/props/motion, not fresh spawn defaults. The optional view-range save chunk
is explicitly admitted by SaveService; old saves/default-range behavior remain.

## Demonstration and honesty about editor evidence

[World Workshop](../projects/world_workshop/README.md) is an ordinary 364-object
project constructed via shipped CLI templates, patches, import/cook/fit and recipes.
The source-art helper only writes glTF. Shared command tests exercise batching,
undo/cancel/atomic rejection/geometry traversal and preserved overrides. The actual
editor loop draws the world/source/UI panels and performs shared commands plus
Play/Stop. Automated input exercises the normal app, cameras/pause/localization,
spawn, region adoption/retirement/revisit/reload and separate-process save/load.
Region automation uses controlled repositioning; human traversal is still required.

Native GUI click automation is unavailable in this environment. No claim is made
that an agent manually created the UI by clicking widgets. The operator workflow
below deliberately covers visual → text/CLI → visual edits, character authoring,
recipe regeneration and usability that programmatic checks cannot accept.
Existing Three Games content/tuning is preserved and used as a consumer workload.

## One combined human review

1. Open `projects/world_workshop/world_workshop.judasproj` in `judas_editor`.
   View → World building / named source. Choose `gallery-0`, change spacing/count,
   Preview/cook → Apply, undo/redo. Change `quarter` profile/segments and a path;
   inspect removals/overrides, then Play and walk on the real ramps/stairs/corridor.
2. Ctrl/Shift-select parent/child or several columns. Try mixed properties, shared/
   individual pivots, local/world edits, folders, parenting, duplicate/reference
   review and one-drag undo/Escape cancellation. Type in fields; shortcuts must stay
   in the editor rather than delete objects or start Play unexpectedly.
3. Apply a named scene edit, save/reopen, amend the same copy with `--edit`/`--patch`,
   review external diff/reload and edit visually again. Try invalid syntax/reference
   or profile in a copy; the last-good scene must survive. Change project input via
   the file source, explicitly reopen the project and confirm binding behaviour.
4. Select the six-part figure; inspect stable skeleton keys, socket and limb target.
   Pick a small chain, Preview fit, inspect boxes/axes, accept, undo. Batch limits/
   resistance only when explicitly enabled; check pre-existing physical intent.
5. Select `Assets/ui/workshop.judasui`: Edit UI document → Runtime canvas preview.
   Change resolution and English/Arabic locale, offset/padding/flow/style, duplicate,
   undo, edit Named source/apply and return to visual editing. Check RTL, wrapping,
   clipping, effective rectangles and pointer focus after runtime layout changes.
6. Play: WASD/sticks + mouse; Space launch; V first/third person; Z two debris
   instances; G ragdoll/return; L locale; Escape pause/resume; R reload. Look toward
   the 800 m turquoise marker, resize and confirm projected HUD alignment.
7. Travel toward −Z: east −45 m → west −95 m → east. Confirm traveller persists.
   F6 save, quit completely, reopen and F7 load; continue. Stop/reload must clean
   runtime state without changing authored content.
8. Launch the moved ordinary package from another working directory. Repeat camera,
   UI/spawn, pause and save/load controls. Review overall authoring usability and
   responsiveness; human visual/interactive acceptance remains authoritative.

Final measured validation, fingerprints and limitations are recorded in
[evidence/m67/REPORT.md](evidence/m67/REPORT.md). Commit/push is authorized as a
provisional checkpoint; the combined human review remains pending. No milestone tag.

# Judas Engine Alpha v1.1.0

Machine version **1.1.0-alpha.1**. Alpha, not Stable/GA. Native 2D game development
and save/UI reliability improvements since published Alpha v1.0.4. Previous tags
and packages remain immutable. The release manifest identifies exact shipped
source and artifact hashes; these notes alone are not publication evidence.

## Native 2D workflow

- Sprites, transparency, source rectangles, pivots, flips, sorting/Y-sort, nearest
  sampling and optional camera pixel scaling through the existing renderer.
- Sprite sheets, frame clips, blending/transitions, events, editor preview/slicing
  and ordinary scene/prefab/save/export integration.
- Layered tile palettes and viewport painting/erasing, Undo/Redo, static merged
  collision and persistent authored tile data.
- Native planar MK2 circles, boxes, capsules, convex shapes and static chains;
  overlap/cast queries and physical collision/trigger events. This is not a second
  physics engine. Planar and 3D physical bodies cannot coexist in one world.
- New Project presets for platformers, free top-down and cardinal grid RPGs;
  editable project JavaScript, native physical authority and script-owned cameras.
- Static straight one-way platforms, blocking-side editor preview and saved
  geometry; grid movement retains pending steps/turns across cold saves.
- Reusable script parallax and sampled attack helpers. Combat, dialogue, battles,
  inventories, enemies and damage remain game-owned policy.
- Four editable learning projects in the SDK/source package: Beacon Grove,
  Tilemap Workshop, Sprite Animation Workshop and Traversal Workshop.
- Human-feedback layout correction: grid obstacles fit their cells; the workshop
  no longer layers them over a wider inherited obstacle. Backdrops cover the room.
- Clearer project setup and one-way labels, with a short current 2D quick start.

## Reliability fixes

- UI callback exceptions are reported at useful locations without silently
  disabling the whole gameplay script slot; lifecycle/failure boundaries remain
  explicit.
- Save/scene coordination exposes busy state and pending-save behaviour with
  project-owned control policy. Saves avoid duplicate snapshot encoding and report
  real elapsed/queued/identity/worker timings. Uncapped harness frames are not
  wall-clock latency; issue #27's seconds-long delay claim was corrected.
- Harness Quit testing can observe ordinary project quit requests. ENTITY
  telemetry reports the actual native pose rather than an authored pose mixed
  with live velocity.
- Tests reject undersized animation fixtures explicitly; layout-save UI testing
  no longer depends on a stale button screen coordinate.

- Release qualification caught and corrected default perspective-camera loading:
  records without the optional new lens extension retain their normal default.
  Explicit malformed extensions still reject atomically. Original failures and
  exact runtime-matrix controls are retained in the camera qualification evidence.

- Qualification also found path-alias failures when authoring scripts or generated
  capsule/cylinder assets. Physical project ownership is still checked, while new
  assets retain the database's path spelling for registration. Windows short
  paths and symlink-opened projects no longer mix canonical and lexical roots.
  Negative controls still reject assets belonging to another project.

## Qualification and compatibility

The operator accepted the demonstrated 2D gameplay, corrected workshop and
pre-publication usability work. Exact-source Linux/Windows preparation and SDK
receipts must still pass before publication. Earlier native Windows 11 and
Android human evidence is retained; this release does not invent fresh device,
controller, audio-hardware or clean-machine certification. Hosted Windows checks
are native build/selected-contract checks, not a GPU trial.

New one-way descriptors use the supported versioned geometry payload; ordinary
non-one-way geometry keeps its old encoding. Existing APIs remain compatible where
practical. Project saves/content/migrations retain explicit identity contracts,
not universal compatibility across arbitrary game updates.

## Limits

Advanced Fluids remains locked in Alpha for unresolved contact-stability issues.
Basic Water and swimming remain independent. Dense-contact performance remains
content dependent. Tilemap solids are static; moving/curved one-way platforms,
drop-through commands and a built-in RPG battle framework are not included.

**Browser export is not included.** Work begins separately on Experimental, hidden
unless `-ExpBrowserSupport=1` is explicitly passed. No browser compatibility is
claimed by a desktop or Android package. See the browser feasibility review.

## Quick test

Open a learning project, Play/Stop, edit a sprite and its independent collision,
paint/Undo a tile, save/reopen, test one-way jumps and grid saves, then export and
run the complete package from another directory. Start with 2D_QUICKSTART.md.

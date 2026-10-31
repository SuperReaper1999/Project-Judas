# Make your first 2D game

These workflows are included in the **Alpha v1.1.0 candidate**.
The older published Alpha v1.0.4 does not contain them. Historical Stage reports describe
what existed at their own checkpoints, not the complete current feature set.

## 1. Choose how your player moves

Open **File → New project**, choose a new directory and name, then select a
**Template**. Click Create. Press **Play** to try it; **Stop** returns to editing.

| Template | Use it for | Starter controls |
|---|---|---|
| New 2D game - top-down | Free movement, adventure/action RPGs | WASD/arrows |
| New 2D game - side-view | Platformers, gravity and jumping | A/D or arrows, Space |
| New 2D game - grid RPG | Cardinal cell-by-cell tile RPGs | WASD/arrows; tap or hold |

These are editable starter projects, not built-in game rules. A grid preset does
not supply battles, dialogue, encounters or an RPG inventory. Those remain game
JavaScript. The scene and controller are yours to change.

## 2. Find the things you edit

- **Hierarchy:** select the player. Right-click an entity to create a child;
  right-click empty space to create at the root. New objects select immediately.
- **Inspector → Transform:** position and rotation. Sprite size and pivot belong
  to its visual component; **Body** owns the real collision dimensions.
- **Scripts:** the starter player has `Assets/game.js` attached. Open the attached
  script or find it in Asset Browser. Edit its exposed properties in the Inspector
  before editing source. Stop Play before changing authored components.
- **View → 2D authoring:** sprite-sheet/frame-animation tools and Tilemap painter.
  These specialized tools need not stay open for ordinary scene editing.
- Mouse wheel zooms the scene view, middle drag pans, and F frames the selection.
  Use the 2D view controls when authoring in XY. Save before running/exporting.

Keep a single camera-owning controller per view. Grid players expose followCamera
so extra actors can be added without competing over the camera. A planar physics
world cannot also contain 3D physical bodies; use **2D** creation entries for
physical objects in these presets.

## 3. Add art, collision and scripts

Import a PNG through Asset Browser. Use a Sprite for an image, or create a sprite
sheet from the registered image for frame animation. Size, pivot, source rectangle,
flip and sorting settings affect presentation; they do not reshape collision.
Transparent image padding can make the visible edge differ from the sprite box.

For solid objects, use native 2D shapes and inspect **Body → Preview collision**.
Top-down actors generally use a small footprint at their feet rather than a body
as tall as their image. Grid obstacles should fit the chosen cell size: a 1.4 m
obstacle can reject an adjacent 1 m destination even if its centre lies on a cell.

For a platform, use **Create → 2D one-way platform**. Its arrow shows the blocking
side. A left-to-right edge allows jumping from below and landing above. These
are static straight segments; moving platforms and drop-through commands are
not part of this preset. Ordinary solid collision remains two-sided.

Create a script through the editor's scripting workflow, or right-click an
existing script in Asset Browser → **Attach to selected object**. Scripts run
through the normal JudasJS lifecycle. Source errors appear in JudasJS tools;
use its debugger/reload controls deliberately rather than restarting blindly.

## 4. Build a level and animate it

Open **View → 2D authoring → Tilemap painter**, create/select a grid and a palette,
choose a tile and paint in the viewport. Enable collision only on intended solid
layers. Save the tilemap draft. Use Undo/Redo rather than deleting your scene to
undo a stroke. One-way segments are separate from tilemap solid rectangles.

The sprite-sheet tool slices image frames, defines clips and timed events, and
previews animation without executing gameplay events. Save the sheet and attach
its animator to your sprite. For attacks, JudasJS can consume sampled animation
events and issue native overlap/cast queries; no sword damage is hardcoded.

Parallax is an editable project script using ordinary sprite layers, not a magic
background flag. Start with the [genre guide](judasjs/2d-genres.md) and its examples.

## 5. Save, test and export

Save scene and project changes. Play/Stop protects authored state; a game's
F6/F7 save/load exists only when its script defines those controls. Use the
normal **File export workflow** to package the saved project, then move the entire
output directory and run it there. A desktop package is not a browser build.
Browser export is currently unsupported; see [the feasibility review](BROWSER_EXPORT_REVIEW.md).

## Working examples

Copy SDK examples into your own writable project folder before editing or saving.
Keep the installed SDK intact so its checksums remain meaningful.

- [Beacon Grove](../projects/beacon_grove/README.md): complete small action/adventure.
- [Traversal Workshop](../projects/2d_traversal_workshop/README.md): 1 selects jump-through
  platforms; 2 selects grid movement; R resets; F6/F7 save/load.
- [Stage 3](2D_STAGE3.md): sprite sheets, frame animation and events.
- [Stage 4](2D_STAGE4.md): tilemap authoring and collision boundaries.
- [JudasJS 2D physics](judasjs/physics-2d.md): actual APIs and controller limitations.

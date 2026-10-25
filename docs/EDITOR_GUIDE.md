# Using the Judas editor

This is the current editor workflow, building on the accepted M71 foundation,
[M72 render controls](M72.md) and later authoring/developer tools. Historical
milestone reports retain their original review status. Physical Windows GPU/input
acceptance remains separate; see [platform qualification](WINDOWS.md).
[M74 retargeting](M74.md) retains its recorded pending human-validation status.
[JudasJS reference](JUDASJS.md) · [Make a game by hand](judasjs/getting-started.md) · [Named authoring](NAMED_AUTHORING.md)

## Workspace

Open `judas_editor` with a `.judasproj` or a scene inside a project. Edit mode has
a central scene view, hierarchy on the left, inspector on the right and asset
browser below. Drag the dividers to resize them. **View → Asset Browser** hides
the bottom panel; **View → Reset workspace layout** restores the defaults.
The layout adapts to smaller windows; the minimum window is 640 × 480.

The toolbar puts **Play**, **Save** and **Create…** first. Wider windows also
show Undo/Redo and Move/Rotate/Scale. **Options…** (or **Transform…** in a smaller
window) holds transform choices, world/local axes and snap. **View…** opens scene
settings, assets, preferences/layouts and navigation help. These actions also
remain in the main menus. Controls adapt to window width and UI scale. The status bar reports
unsaved edits and operation results. Hover truncated names to see their full text.

Panel sizes and auxiliary-window placement are saved in SDL's per-user
`Judas/Editor` preferences directory, independently of project files. Invalid
preferences fall back to defaults. There is no docking framework or workspace
sharing requirement.

## Move around and author objects

| Task | Control |
| --- | --- |
| Pan | Middle-button drag inside the scene view. |
| Zoom | Mouse wheel inside the scene view; wheel up moves closer, down moves away. F sets the focus depth. |
| Look / fly | Right-button drag; hold WASD and Q/E, Shift for faster flight. |
| Frame selection | F, hierarchy **Actions → Focus selection**, or right-click **Focus**. |
| Select | Left-click geometry or a hierarchy row; Ctrl adds, Shift selects a hierarchy range. |
| Transform | W / E / R: move / rotate / scale; X changes world/local axes; Ctrl temporarily snaps. |
| Undo / redo | Ctrl+Z / Ctrl+Y. Shared transform/text/field helpers group a gesture into one edit; some specialized controls still commit each change. |
| Duplicate / delete | Ctrl+D / Delete, or hierarchy actions. |
| Save | Ctrl+S or **File → Save scene**. Unsaved scenes use a path prompt. |

Start camera gestures in the scene view. Release, Escape or window focus loss
ends capture. Camera navigation never edits scene objects. The current camera
supports wheel dolly, pan and look/fly; it does not provide a separate orbit mode.
Wheel zoom keeps the focus point and lens unchanged and ignores panel scrolling
and active editing/camera gestures. Viewport selection tests visible box/sphere
geometry and mesh triangles in resolved hierarchy transforms, so large floor or
parent bounds cannot steal small-child clicks. Empty objects retain a small pivot
proxy that takes priority only when no visible surface is hit. The hierarchy can
select obscured objects. Edit-mode meshes are picked in the same rest pose as the
authoring view; transparent texture holes are not tested. Mesh geometry is cached
on the first click and refreshed when its source file changes.

Use **Create** for ordinary geometry, lights and other starter components.
You can also right-click empty space in the hierarchy and choose the object type
directly, creating a root object at the scene-view focus. Right-click an existing
row for its **Create child** submenu; children start at their parent's local origin
with identity rotation and unit scale. Both select the new object immediately,
use the ordinary component defaults and support Undo/Redo as one creation step.
Scene creation is disabled in Play.
Use the inspector's **Add component…** for scripts, motors, animation, bodies,
UI and the other engine primitives. Common inspector fields have labels above
their controls; some specialized controls retain side labels. Asset selections
show full paths/IDs in tooltips and missing/unknown status below the selector.
The hierarchy supports search, renaming, multiple selection, grouping and
copy/paste through the existing authoring tools. Its **Create…** footer offers
root creation; right-click **Set parent** lets you search for a parent or choose
Scene root. Reparenting preserves world placement, supports Undo/Redo and rejects
cycles. The existing right-click child creation stays relative to its parent.

### Add gameplay to an object

Select one object. Its name and always-visible **Transform** come first, followed
by **Components**, **Add component…** and attached components. **Add gameplay
script** holds the script creation/attachment actions; it starts open for an
object without scripts. Component sections are collapsed until expanded; newly
added components open automatically. **Object settings** holds parent, tags,
render layer and visibility. Clipboard, prefab and runtime diagnostics stay in
separate collapsed sections. Right-click a component heading to remove it.
The Add component selector supports a name search and alphabetical results.

1. Expand **Add gameplay script** if needed. Click **Create script & attach**, type a file name and choose **Create & edit**.
   This writes `Assets/Scripts/<name>.js`, registers its normal metadata, attaches
   it and opens the built-in source editor. Existing files are never overwritten.
2. Write gameplay in the source window and press **Save source**. Close that
   window when you want the player to capture the mouse.
3. Save the scene to retain the attachment, then press **Play**. During Play,
   use **Reload scripts** deliberately after saving source edits.
4. For a registered script already in the project, use **Attach existing script**.
   Each attached script has **Edit script** and its exposed inspector properties.

Undoing an attachment leaves its source asset on disk. Undo is not file deletion.
The empty behaviour-slot workflow remains under **Advanced script slots** for
specialized authoring, but is not needed for ordinary scripting.

### Make a controllable player

Use hierarchy right-click or **Create → First-person player (JS + motor)** or
**Third-person player (JS + motor)**. These author the same player with a different
starting-view property: CharacterMotor, a capsule visual and editable project JS
in `Assets/Scripts/Player*.js`. There is no native FPS controller.

Save the scene and press Play: **WASD** moves, mouse looks, **Space** jumps,
**V** switches first/third person and **Escape** releases/captures the mouse.
Escape in this minimal template releases control; add the optional pause example
below for a real paused world and Settings menu.
The third-person camera sphere-casts against physical geometry to avoid walls.
Movement/camera policy and speed/jump properties belong to the project script.
The initial third-person visual is a capsule, not an animated character model.

The template requires the named logical inputs in Project Settings → Input and
explicitly rejects legacy gameplay. It disables only the exact unedited rotating
box starter script that otherwise owns the camera. Other camera scripts are
preserved; choose one main-camera owner yourself. Gravity and ground collision
remain scene-authored; the template does not invent a ground or gravity field.

### Capsules and cylinders

**Create → Capsule / Cylinder** and the hierarchy menus generate reusable OBJ
and cooked convex-collision assets under `Assets/Generated/Primitives`. They are
ordinary meshes and convex bodies, not analytic runtime shape kinds. Defaults
are static, one metre wide and two metres tall on local Y. Change Body → Motion
to Dynamic when appropriate. The capsule's 98-vertex convex approximation fits
the existing collision-cook limit; it is not a mathematically exact capsule.
Repeated instances share assets. Edited generated files are never overwritten;
use normal mesh/collision authoring for customized geometry. CharacterMotor's
own analytical capsule is a separate movement primitive.

New/Open/project changes and closing the editor prompt before discarding a dirty
scene. Choose **Save and continue**, **Discard** or **Cancel**. A failed save
keeps the action pending. This protects scene edits; project settings and external
script files use their own explicit save workflows. Export reads saved files.

## Assets and scripts

Double-click a script in **Assets** to open it directly in the built-in source
editor. Its right-click menu also offers **Edit script**, **Inspect asset** and
**Open file externally**. Other asset types keep their double-click inspection
behaviour. Missing/read-only script sources cannot open a writable editor.
During Play, source saving remains separate from deliberate script reloading;
opening a file does not change the running code. Existing unsaved source edits
remain protected when switching files.

The asset browser puts **Import assets…** at the top of its **Assets** tab. Choose a source, then
review its destination inside Assets and click **Import file**. The source is
preserved. Use **Filters…** for type/missing filters; the search matches names,
paths, IDs and sources without case sensitivity. A selected script exposes
**Edit script** and **Attach to selected object** directly; attachment requires
one selected object in Edit mode. These also appear on its right-click menu.

The asset browser has three tabs:

- **Assets**: search by name, ID or source; filter type/missing state; rescan.
- **Selected asset**: inspect/edit the selected resource, expand **Organize asset** to
  rename/move/remove it, copy its
  path, or request **Open file** in your configured application.
- **Import & tools**: track existing files and use the current import/bake tools.

Select an object and use **Create script & attach**, or **Attach existing
script**. The first action creates and tracks a JavaScript file in
`Assets/Scripts`; its **Create & edit** button opens the source editor. Attached
scripts have an **Edit script** button. You can also import existing `.js` files
through the asset browser and attach them through Scripts. Edit game rules
externally or use **View → JudasJS developer tools → Edit source**.
The built-in source editor saves explicitly and refuses to overwrite a file
changed externally. **Check project scripts** checks registered modules in Edit
mode. During Play, **Check sources** compiles/links and **Reload scripts** applies
a deliberate safe-boundary session replacement. **Keep JSON state** retains only
supported instance state; constructors and `start` run again. Stop/Play provides
a fresh authored world. Disk changes never hot-reload automatically.
Use Stop/Play before cold saves or online sessions after a reload, as described
in [debugging, stepping and reload boundaries](M94_DEVELOPER_TOOLS.md).
Local file/document opening uses the
OS default handler, which must be configured to open Markdown/scripts usefully.
Use **View → JudasJS reference** or **Making your first project** for the offline
reference and [hand-authoring guide](judasjs/getting-started.md).

## Animation retargeting

Open **View → Animation tools → Animation retargeting**, or the retargeting button in **Asset
Browser → Import & tools**. This is an authoring workspace: it creates no game
world, runs no scripts and does not mutate the open scene or Play state.

1. Choose a project-relative animation source and a target `Imports/*.judasimport`
   recipe. Set the source's unit/basis conversion only when source metadata needs
   explicit conversion, then **Load source + target on worker**.
2. Create/load an `Imports/*.judasretarget` profile. Add explicit source/target
   mappings with **full hierarchy keys**. Short ambiguous bone names do not bind.
   Unmapped target helpers retain their reference/rest local transform.
3. Set model-frame alignment, positive translation scale, selected translation
   channels and any static local reference corrections. These controls calibrate
   correspondence; they do not rewrite the mesh's bind pose or skin weights.
4. Choose the source take, ordinary output name, trim, sample rate and explicit
   root policy. **Validate profile**, then **Refresh comparison (CPU only)**.
   Source and target share preview time and scale; play/pause, scrub, skeleton
   axes and reference-pose inspection are independent of the game's camera.
5. **Save profile** and **Save recipe operation** separately. **Bake / reimport
   target model** uses the normal bounded import job and guarded publication.
   Existing target geometry, materials and native clips remain ordinary content.
   Output-name collisions fail unless an existing *retarget operation* is
   explicitly selected for replacement; a native clip is not silently replaced.

**Undo draft / Redo draft / Cancel draft edits** affect the local mapping and
operation draft, not scene Undo or filesystem writes. Changed drafts mark the
comparison stale. Closing the panel or changing project cancels unpublished
work; failed or stale publication retains the last-good cooked target. Entering
Play suspends the preview and defers publication until Edit; it does not cancel
the authoring jobs. Source decoding and comparison baking run on the existing
worker pool; GPU upload and drawing stay in Renderer on the editor thread.

The game uses the resulting **ordinary named target clip** with the existing
animation API. It needs no runtime profile, source rig or retargeting service.
See [M74's transfer convention, bounds and limitations](M74.md) and
[model import](MODEL_IMPORT.md) for the matching CLI and recipe format.

For ordinary clip preview, scrubbing and event-track authoring, use
**View → Animation tools → Animation timeline**; see the [animation workflow](M95_ANIMATION_WORKFLOW.md).
Retargeting and the runtime animation timeline are separate tools.

## Terrain authoring and live edits

**View → Terrain and world tools → Terrain sculpt / paint** opens shared-source height/radial terrain editing.
Select a terrain and open its source, or create/cook a new source and instance.
Raise/lower/flatten/smooth/paint use the current draft triangles; Save source and
Cook are separate. Draft graphics are not published collision. This authoring
panel is unavailable during Play. See [M75 workflow, units and lifetime](M75.md#editor-workflow).

[Volumetric terrain](M84_VOLUMETRIC_TERRAIN.md) uses its own source/cook workflow.
Project JavaScript can separately request bounded live volume edits through
[runtime terrain APIs](judasjs/terrain.md); that does not enable the authoring
brush during Play or automatically save runtime changes into the authored source.

## Lighting, visibility and materials (M72)

In the ordinary object inspector, **Entity render visible** is a local render
gate. Under Render, **Render component visible** and imported **Part visible**
remain independent. Hiding geometry leaves hierarchy/inspector access available
and preserves physics/scripts/audio/animation. Children keep their own gates.

**Whole renderable material** edits authored whole-instance factor/map overrides.
Numeric material slots and each imported **Part material** select shared material
assets and more specific overrides. Factor controls include **Base colour / opacity (linear)**, **Alpha mode**
(Opaque/Cutout/Alpha blend), **Cutout threshold**, PBR/emission/normal/occlusion/UV
settings and the five texture roles. Texture selectors offer inheritance, explicit
removal or a registered asset. **Clear parameter and texture overrides** restores
the inherited factors/maps. Shared material-source saves remain deliberate disk
operations; instance authoring uses normal scene Undo/Redo.

Scene settings expose authored sun enable/intensity/direction/colour, ambient and
the existing environment/background/exposure controls. Direction points toward
the source in world coordinates. Exposure is independent of lighting. There is
no native day/night clock; project scripts choose transitions.

During Play, **Runtime render preview** exposes live visibility and whole/numeric/
stable-part material targets, source selection, factors/maps and **Clear runtime
material binding**. The scene settings runtime section includes sun/environment
controls and **Reset runtime environment**. **Undo preview / Redo preview** use
a separate bounded history; they do not change authored Ctrl+Z history or silently
save runtime values to the scene. Start/Stop clears this preview history and Stop
reconstructs authored state. See [exact API/reset contracts](judasjs/materials.md)
and the [Render Control Lab](../projects/render_control_lab/README.md).

## Play, stop and ship

**F5 / Play** instantiates the authored scene. Running Play uses the normal
full-window runtime view; authoring rails collapse. A paused game can expose
runtime inspection and render previews. Project scripts own modern input, camera and pause meaning;
historical projects can retain legacy controls. **Stop / F5** discards runtime
state and returns to the authored scene.

Select the startup scene and input bindings in **File → Project settings**.
**Run project** launches the separate runtime. **Export project (Release)**
creates a movable runtime package; see [export](M38.md) and [Windows](WINDOWS.md).
Save scenes, settings and source before exporting. JavaScript completion comes
from [judas.d.ts](judas.d.ts), with setup in [practices](judasjs/practices.md).

## Review the current editor

1. Resize the editor and its dividers; confirm labels, scene and assets remain usable.
2. Select, frame, pan, fly and transform objects; Undo/Redo the complete gesture.
3. Edit a scene and try New/Open/close; Cancel preserves it, Save/Discard continue.
4. Track a script, open its file and reference, attach it, then Play/Stop.
5. Save, reopen and export a small project.

Automated evidence is in [M71 editor polish](evidence/m71/editor-polish/README.md)
and [wheel/picking follow-up](evidence/m71/editor-picking-zoom/README.md).
Those receipts describe accepted M71 work. [M72 review](M72.md#human-review)
adds lighting, hiding and material checks; current results are reported there.
Offscreen checks do not establish physical desktop capture or human usability.

## Local mods and game kits

Open **View / Local mods / game kits**. Export only a game's explicitly published
kit. Create a separate kit-bound project and open it normally. Published Kit/base
references are read-only; fork terrain into Imports before sculpting. The ordinary
Terrain sculpt / paint panel, Save/Cook and navigation Bake remain the workflow.
The manifest draft has validation, Undo/Redo, cancel/reopen and guarded Save;
Package is a separate publication. The same panel inspects/installs local packages,
requires explicit exact-payload script trust, selects versions and launches an
installed game with or without mods. Selection changes apply next session/launch.
See [full contracts, limits and CLI commands](MODDING.md).

## Build-channel update notifications

The optional background check compares this build's exact revision with its
`main` or `Experimental` GitHub channel. **New Judas build available** reports
newer commits, including unnumbered fixes. Use **View → Check this channel for
updates** to retry, and **View changes** to open the comparison in your browser.
An older or diverged remote history is not reported as an update.

The check uses a bounded read-only HTTPS request through optional system `curl`;
GitHub CLI and sign-in are not required. Missing utility, offline access, service
limits or unknown/Local provenance leave the editor usable and report an
unavailable manual check. No update is installed and no repository is written.
See [build identity and channels](BUILD_RELEASES.md#update-notification).
The [original milestone-notice receipt](evidence/editor-milestone-notice/RESULTS.md)
describes its older `gh` implementation, not this current path.

## Project documents and first-project workflow

### Personal appearance and saved layouts

Open **Edit → Editor preferences** (also available in **View**).

- **Appearance:** choose Light or Dark, pick an accent preset or your own accent,
  background and text colours, and adjust UI scale. Changes preview on the next
  frame. Switching Light/Dark restores that theme's base colours while retaining
  the scale; **Reset appearance** returns to the default dark palette.
- **Layouts:** resize the hierarchy/inspector/asset rails, choose visible tools,
  and move or resize floating tool windows. Enter a name and **Save as new
  layout**. Choose a saved layout and **Apply selected**, **Update selected** or
  **Delete selected**. Updating is explicit; saving a new name never overwrites
  an existing one silently. Deletion asks for confirmation.
- **Reset panel layout** restores default rail sizes and visible panels. It does
  not erase saved layouts or reposition floating tools. Applying a saved layout
  restores the tool positions included in that snapshot.

The main hierarchy and inspector remain fixed rails; this is not free-form
docking. Layouts store panel visibility, dimensions and floating-tool positions,
not scene state, selection or the editor camera. Smaller windows clamp the rail
sizes to keep the viewport usable.

Named save/update/delete/apply actions save preferences automatically. For colour,
scale and manual panel changes, use **Save preferences**, or close the editor
normally. Preferences are personal, shared across projects, and stored in the
platform's Judas/Editor user-data directory (`workspace.json` and `windows.ini`),
never in a game project or export. Existing workspace preferences migrate when
saved. Replacing `workspace.json` retains its previous bytes in `.bak`; failed
saves report an error and preserve the current file. Invalid preferences fall
back to defaults with a diagnostic in the preference window.

The main **Create** menu and hierarchy creation menus have a search field.
Search names or descriptions, for example `player`, `light` or `swimming`; hover
an item for its purpose and defaults. Each opening starts with the full list.
Select one object and press **F2**, or use **Edit → Rename**, to rename it in the
hierarchy. Enter confirms; Escape cancels. Text fields retain keyboard ownership.

### Optional pause menu and rebindable controls

In Edit mode, use **Create → Pause menu + controls (JS + UI)**, or the same
hierarchy right-click item. This adds an ordinary, editable UI asset and
`PauseMenu.js`; it does not change existing scenes automatically. Save the scene
and press Play. **Escape** opens Pause; **Settings** lists the current project's
logical controls and their physical bindings.

Select a binding, choose **Change selected binding**, find a keyboard, mouse or
gamepad control, then choose **Apply selected control**. This example uses a
searchable control list rather than a “press any key” capture dialog. **Restore
project defaults** restores the map from Project Settings. UI navigation bindings
are protected in the example to avoid making the menu inaccessible; edit the
project script to change that policy. Keyboard navigation uses arrows and Enter.

The generated first/third-person player cooperates with this menu. The menu's
modal state pauses normal fixed-step gameplay while UI callbacks continue.
Custom player/camera scripts should likewise respect the modal state rather than
recapturing the mouse while a menu is open. Restart reloads the current scene;
Quit exits the standalone game and ends Play when running inside the editor.

Bindings are runtime preferences, not edits to the project file. This example
keeps them in ordinary script save state when the game saves; it does not create
an automatic preferences file on every exit. A production game should choose its
own save/preferences policy. The example targets the main project input map;
local-player-specific maps remain owned by their respective players. See
[runtime rebinding](judasjs/input.md) and the editable
[pause script](judasjs/examples/pause-menu.js).

New project offers **Visible script starter** (an ordinary tracked `Assets/game.js`
and render-only owner/camera) or **Empty scene**. Use Browse to choose a folder.
Scene/project Open and Save As, asset source import and export destinations also
have a bounded integrated file picker. It caches listings until navigation or
Refresh; choosing a path does not create/overwrite content.
Single-click selects an entry without closing the picker. Double-click a folder
or select it and press **Open folder** to browse inside it. **Parent** goes up;
enter a location and press Enter or **Go** to jump there. **Choose folder/file**
confirms the selection; **Cancel** leaves the original path unchanged.

Project Settings has its own dirty indicator and settings Undo/Redo. Scene undo
still affects scenes. Closing/replacing a project offers Save/Discard/Cancel;
failed/conflicting saves preserve live changes. Run/Export read saved content,
with **Save all and continue / Use saved version / Cancel** when edits exist.
Choosing the saved version does not discard the live edits. See
[the first-project walkthrough](FIRST_PROJECT.md) and
[current correction scope](PREALPHA_FINAL_CORRECTIONS.md).

## Finding specialized tools

Ordinary scene editing keeps specialist windows closed unless your saved layout
opens them. **View → Terrain and world tools**, **Animation tools** and
**Diagnostics and extensions** group the existing terrain, world, animation,
profiler and mod tools. JudasJS developer tools remain directly under View.
Use **Editor preferences → Layouts** to save layouts for different tasks,
including panel visibility, rail sizes and floating-window placement. Themes,
custom colours and UI scale apply to the common editing controls.

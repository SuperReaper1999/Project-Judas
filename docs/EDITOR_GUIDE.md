# Using the Judas editor

Current M71 candidate; human review and Windows validation remain pending.
[JudasJS reference](JUDASJS.md) · [Make a game by hand](judasjs/getting-started.md) · [Named authoring](NAMED_AUTHORING.md)

## Workspace

Open `judas_editor` with a `.judasproj` or a scene inside a project. Edit mode has
a central scene view, hierarchy on the left, inspector on the right and asset
browser below. Drag the dividers to resize them. **View → Asset Browser** hides
the bottom panel; **View → Reset workspace layout** restores the defaults.
The layout adapts to smaller windows; the minimum window is 640 × 480.

The toolbar has **Play**, **Save**, transform tools, world/local axes and snap.
Smaller windows group transform choices under **Tools**. **Scene** opens global
scene settings; **Help** shows navigation and shortcuts. The status bar reports
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
| Frame selection | F, hierarchy **Focus**, or right-click **Focus**. |
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
Use the inspector's **Add component…** for scripts, motors, animation, bodies,
UI and the other engine primitives. Common inspector fields have labels above
their controls; some specialized controls retain side labels. Asset selections
show full paths/IDs in tooltips and missing/unknown status below the selector.
The hierarchy supports search, renaming, multiple selection, grouping and
copy/paste through the existing authoring tools.

New/Open/project changes and closing the editor prompt before discarding a dirty
scene. Choose **Save and continue**, **Discard** or **Cancel**. A failed save
keeps the action pending. This protects scene edits; project settings and external
script files use their own explicit save workflows. Export reads saved files.

## Assets and scripts

The asset browser has three tabs:

- **Assets**: search by name, ID or source; filter type/missing state; rescan.
- **Selected asset**: inspect/edit the selected resource, move/remove it, copy its
  path, or request **Open file** in your configured application.
- **Import & tools**: track existing files and use the current import/bake tools.

Place `.js` files under the project's assets directory, rescan and track them,
then attach their stable asset IDs through an entity's Scripts component. Edit
game rules in those files; Stop and Play again to reload scripts. There is no
embedded code editor or script hot reload. Local file/document opening uses the
OS default handler, which must be configured to open Markdown/scripts usefully.
Use **View → JudasJS reference** or **Making your first project** for the offline
reference and [hand-authoring guide](judasjs/getting-started.md).

## Play, stop and ship

**F5 / Play** instantiates the authored scene. Running Play uses the normal
full-window runtime view; authoring rails collapse. A paused game can expose
read-only inspection. Project scripts own modern input, camera and pause meaning;
historical projects can retain legacy controls. **Stop / F5** discards runtime
state and returns to the authored scene.

Select the startup scene and input bindings in **File → Project settings**.
**Run project** launches the separate runtime. **Export project (Release)**
creates a movable runtime package; see [export](M38.md) and [Windows](WINDOWS.md).
Save scenes, settings and source before exporting. JavaScript completion comes
from [judas.d.ts](judas.d.ts), with setup in [practices](judasjs/practices.md).

## Review this candidate

1. Resize the editor and its dividers; confirm labels, scene and assets remain usable.
2. Select, frame, pan, fly and transform objects; Undo/Redo the complete gesture.
3. Edit a scene and try New/Open/close; Cancel preserves it, Save/Discard continue.
4. Track a script, open its file and reference, attach it, then Play/Stop.
5. Save, reopen and export a small project.

Automated evidence is in [M71 editor polish](evidence/m71/editor-polish/README.md)
and [wheel/picking follow-up](evidence/m71/editor-picking-zoom/README.md).
Offscreen checks do not establish physical desktop capture or human usability.

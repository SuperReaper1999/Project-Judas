# Editor V2: workspaces, ownership and review

Editor V2 is the current **Experimental development preview**. Scoped Linux
automated qualification with Scripting V2 passed; combined operator acceptance
is pending. This page does not declare Alpha v2.0.0, update main or extend
historical platform receipts.
Use the guide packaged with an installed SDK when its editor predates this work.

[Practical editor guide](EDITOR_GUIDE.md) · [First project](FIRST_PROJECT.md) ·
[Scripting V2](judasjs/scripting-v2.md) · [Platform scope](ALPHA_SUPPORT_MATRIX.md) ·
[Qualification receipt](EDITOR_V2_QUALIFICATION.md)

## Everyday workflow

The default Scene workspace has Hierarchy on the left, Inspector on the right,
Scene/Game tabs in the centre and Asset Browser/Console tabs below. Drag tabs to
dock, split or float panels. Close tools independently and reopen them through
the **Window** menu. The toolbar's **View…** popup offers quick view settings;
the main menu uses Window categories and Help links.

- **Hierarchy:** **Add object** at the top creates a root; right-click empty
  space offers the same creation menu, and right-click an object for **Create
  child**. Creation selects the result. Type icons, indentation/branch guides
  and the accent-coloured selection distinguish the scene tree. Search, rename,
  parenting, multiple selection and Undo/Redo remain available.
- **Inspector:** the actual editable object name is the primary line, beside a
  type icon with a quiet type subtitle. Name and compact **X/Y/Z** Transform rows
  stay above the scrolling component area. Components use separate bordered
  cards; their **…** menu exposes the existing removal action, also available
  by right-clicking the heading. **Add component…** is anchored at the bottom.
  Script attachment remains explicit, and advanced object, prefab and runtime
  information stays accessible in collapsed groups. Enable controls remain in
  the components that support them.
- **Asset Browser:** use folders and breadcrumbs to navigate; switch between
  list and grid views. Grid is the initial view, with labelled vector type icons.
  Import/Create actions,
  search and type/missing filters narrow the registered assets. Double-click a
  script to edit it; select a script for **Attach to selected object**.
  **Selected asset** retains material/cook/dependency tools;
  **Import & tools** retains model, skeleton, motion and collider workflows.
- **Judas Tools:** open **Window → Tools → Judas Tools** for a small floating
  utility which can also be docked. Its Scripts, Build and Utilities tabs expose
  project scripts, deliberate reload/source checks and existing build/settings
  actions. The source/debugger window remains named **JudasJS developer tools**;
  the launcher is not a second script editor or a new compiler/runtime.
- **Console:** presents existing runtime diagnostics and captured output, with
  search and source-opening actions. Script output capture still uses the
  existing debugger controls; errors do not require output capture. This pass
  does not introduce an always-on second logging/runtime system.

The workflow strip puts **Scene**, **Playtest**, **Scripting**, **Animation**,
**Audio** and **UI** above the ordinary Play/Stop and authoring controls. Playtest
reveals/focuses Game; it does not start a second world or replace the Play button.
Scene and the specialist buttons apply existing workspace arrangements. Smaller
windows use shorter labels or a Workflows menu; the workspace selector remains
available. Audio selects project audio settings, not a new sequencer; UI document
authoring remains in the Asset Browser. Vector icons and heading emphasis follow
the current Light/Dark/custom appearance rather than changing game materials.

Opening a scene initially frames its world-transformed player-start or
CharacterMotor area when present. Otherwise it chooses visible content or a
spatial fallback, instead of blindly framing a first-serialized settings owner
or planet centre. The 3D distance uses a size hint; 2D keeps its existing framing.
This changes only the editor camera, not authored transforms, selection or physics.

**Scene** is the authoring camera and observes the same runtime during Play,
with authored values read-only. **Game** displays that single played world and
its actual HUD. Play/F5 starts that world, Stop/F5 returns to authored state.
Game-panel visibility does not own simulation lifetime. Focusing a text editor
must not send its typing to the player. Docking/resizing changes the presentation
target and input mapping, not the world's fixed-step policy.
Scene/Game targets currently render at **1× logical UI resolution**. On a HiDPI
display this is not native backing-pixel resolution; a sharper native-density
target policy is outside this pass.

## Appearance, layouts and detachment

**Edit → Editor preferences** retains Light/Dark themes, primary/secondary
accents, custom colours and UI scale. Layouts capture panel visibility, docking,
tabs and floating-window placement. Save, apply, update and delete named
snapshots deliberately; **Reset panel layout** restores the Scene workspace
without deleting named snapshots. Off-monitor floating tools are recovered into
available desktop space.

Linux image review exercised Dark at UI scales 0.85, 1.0 and 1.5, and Light at
1.0 and 1.25. These are automated captures reviewed by the agent, not operator acceptance.
These are scoped checks, not human acceptance or universal monitor/DPI coverage.

Preference schema **3** adds workspace identity, docking state and independent
panel visibility. Readers accept schemas 1/2: appearance and named layouts are
retained, and layouts without a dock graph receive a fresh V2 arrangement. This
is an editor-layout migration, not a conversion of scene or project content.
Preferences remain personal SDL `Judas/Editor` data (`workspace.json` and
`windows.ini`), outside exported projects. Scene state, selection and the editor
camera are not part of a layout snapshot. Failed preference saves retain the
previous file and report the error; replacement retains a `.bak`.

Docking and in-window floating are distinct from native OS-window detachment.
The editor enables native platform windows only for SDL **x11** and **windows**.
Native Wayland and other backends retain the in-window path. On a compatible
Linux desktop, `SDL_VIDEODRIVER=x11 ./judas_editor` requests X11/XWayland for that
launch only. Linux native detached Scene/Game qualification passed its scoped
automated tests; Windows Editor V2 has not been rerun. Existing Windows acceptance
does not qualify this new integration. Android/browser exports are game-runtime workflows, not editor
distributions.

## Source ownership

The shell reuses the existing document, engine host and runtime. It does not add
a scene model, physics world or scripting engine per panel or platform window.

| Owner | Responsibility |
|---|---|
| [EditorApplication](../src/editor/EditorApplication.cpp) | Startup/shutdown and frame coordination over explicit service owners. |
| [EditorShell](../src/editor/EditorShell.cpp), [EditorWorkspace](../src/editor/EditorWorkspace.cpp) | Menus/toolbars/status, dock host, preset arrangement and platform-window presentation. |
| [EditorPanelRegistry](../src/editor/EditorPanelRegistry.cpp), [EditorContext](../src/editor/EditorContext.h) | Setup-time panel descriptors, stable ordering, phase/visibility dispatch and frame-scoped borrowed services. |
| [EditorDocument](../src/editor/EditorDocument.h) | Authored scene, selection, mutations and undo history. |
| [EditorProjectWorkflow](../src/editor/EditorProjectWorkflow.cpp), [EditorRequestWorkflow](../src/editor/EditorRequestWorkflow.cpp), [EditorExportWorkflow](../src/editor/EditorExportWorkflow.cpp) | Project/document commands, file/confirmation state, request application and export. |
| [EditorPlaySession](../src/editor/EditorPlaySession.cpp) | Start/Stop and exactly one RuntimeWorld/InteractivePlay session. |
| [EditorSceneViewport](../src/editor/EditorSceneViewport.cpp), [EditorViewportPresenter](../src/editor/EditorViewportPresenter.cpp) | Camera, picking, gizmos and scene preview; Scene/Game images, render targets and native-window/input coordinate mapping. |
| [HierarchyPanel](../src/editor/HierarchyPanel.cpp), [InspectorPanel](../src/editor/InspectorPanel.cpp) | Actual hierarchy and inspector bodies, with document actions through existing services. |
| [AssetBrowserPanel](../src/editor/AssetBrowserPanel.cpp), [AssetListPanel](../src/editor/AssetListPanel.cpp), [AssetDetailsPanel](../src/editor/AssetDetailsPanel.cpp), [AssetImportTools](../src/editor/AssetImportTools.cpp) | Browser tabs, folder/list/grid interaction, selected-asset tools and import authoring. |
| [AssetBrowserModel](../src/editor/AssetBrowserModel.cpp), [AssetPublicationWorkflow](../src/editor/AssetPublicationWorkflow.cpp), [EditorAssetPreview](../src/editor/EditorAssetPreview.cpp) | Deterministic folder/filter projection, safe completion publication and render-only resource previews. |
| [EditorConsolePanel](../src/editor/EditorConsolePanel.cpp), scene settings, profiler, streaming and UI-asset modules | Their own panel bodies and local drafts; no private application dependency. |
| [EditorToolPanels](../src/editor/EditorToolPanels.cpp), existing specialized editors | Registration/adaptation of scripting, terrain, AI, animation, sprite/tilemap and mod tools. |
| [EditorPreferences](../src/editor/EditorPreferences.cpp) | Personal appearance, visibility and versioned named layouts. |

`EditorContext` is a borrow for the current draw, not a retained global model.
Panel drafts belong to their panel-owned state; shared selection, project
services and persistent visibility remain explicit. Specialized tools keep their
existing implementation when it is already cohesive. This is an incremental
decomposition, not a claim that every component editor is now independent.

Completed cook/import results are pumped by the authoring workflow independently
of Asset Browser visibility. Publication remains Edit-only and rejects a result
belonging to a previous project. Hiding a panel must not stall or prematurely
publish asynchronous work.

## Add or change a panel locally

An existing panel's implementation belongs in its own source, not the application
frame. A new panel supplies a registration function owning its descriptor and
draw callback, then the appropriate setup catalogue calls that function.
[Core panels](../src/editor/EditorCorePanels.cpp) and
[specialized tools](../src/editor/EditorToolPanels.cpp) are separate catalogues.

A descriptor declares a stable ID, visible title, Window-menu category, allowed
phases and an optional visibility member. The registry rejects duplicate IDs,
invalid descriptors, registration during drawing and recursive dispatch. The
draw callback borrows `EditorContext`; it must not retain it or create a runtime.
Use `BeginEditorPanel` for dock-aware windows, and pair every ImGui Begin with
End even when its content is collapsed.

The Console is the focused extension proof: its registration lives in its own
module and is called from the core catalogue. Qualification must demonstrate
that enabling it changes only the panel/catalogue wiring while the application
frame remains byte-identical, and that the registered callback actually draws.
Source locations alone are not that proof.

Build registration and persistent preference visibility are still explicit.
Adding a panel is not a dynamic plug-in loader or automatic schema generator;
if it needs saved visibility, add the named preference member/mapping and tests.
Do not add a panel-specific branch to the main frame or an unrelated panel.

## Shared Judas controls and model discovery

[`EditorDesign`](../src/editor/EditorDesign.h) owns reusable, custom-painted
actions, toggles, selection/tree rows, disclosure headers, search fields, inset
fields and padded surfaces. It uses ImGui's existing input, keyboard navigation,
disabled state, identity and docking infrastructure. It owns no project data or
runtime. [`EditorWidgets`](../src/editor/EditorWidgets.cpp) retains authoring and
Undo transaction boundaries; [`EditorVisuals`](../src/editor/EditorVisuals.h)
supplies semantic vector icons. Panels compose these controls rather than
duplicating their rendering or depending on default widget appearance.

Hierarchy displays actual scene parenting in parent-first order, with folding
and automatic reveal after Scene selection. Sibling order still follows the
authored scene; it does not rewrite serialization or physical ownership. Create
child creates a real scene child, while creation in empty space creates a root.

A complete skinned character can be one scene object. Its skeleton and mesh
parts are model internals, not missing scene children. Use **Inspect model** in
the object's Inspector (Edit mode), then **Shared skeleton** in Asset Details.
The model node list is read-only and follows the imported parent hierarchy.
Runtime socket attachments and script-held entity references do not become
authored scene children. They remain inspectable through their relevant
component/script tools; this view does not invent a unified attachment tree.

Asset grids reuse ResourceManager for asynchronous texture/sprite-sheet
previews. Hidden browsers and Play release preview demand. The browser allows
two pending previews, starts at most two per frame, pins at most sixteen ready
entries and uses a ready-pin budget of the smaller of 32 MiB and one quarter of
the configured resource budget. It currently requests full resource textures:
these limits do **not** strictly bound transient image-decoding memory.

## Scripting, breakpoints and lifetime boundaries

[Scripting V2](judasjs/scripting-v2.md) still provides one QuickJS session for the
played world, shared module registration, the established callback order and
ownership checks. Editor V2 uses the existing script slots, source editor,
breakpoints, stepping, inspection, deliberate reload and error reporting.
No conversion tool, new script language or API break is introduced by docking.

When QuickJS is suspended at a breakpoint, the editor draws the safe shell and
debugger. It must not advance physics, publish asynchronous world changes,
save/load, reload recursively or re-enter game JavaScript from that suspended
stack. Continue/step use the existing debugger commands. Native platform windows
share the editor's ImGui context and restore the main graphics context after
rendering; they do not own additional game sessions.

## Qualification and human review

The [Linux qualification receipt](EDITOR_V2_QUALIFICATION.md) retains commands,
source identity, results and limitations. Recorded scoped passes include:

| Area | Observed automated result |
|---|---|
| Native detached Scene/Game / embedded views | 75 / 60 checks passed. |
| Console / application lifecycle / workspaces / asset browsing | 16 / 22 / 21 / 17 checks passed. |
| Scripting registry / module acceptance / editor workflow / presentation | 20 / 8 / 70 / 44 checks passed. |
| Native pointer/input mapping | 51 checks passed. |
| Packaged SDK | 17 tools and 32 operations checked; project creation, Play/Stop and two relocated standalone exports exercised. |
| Separate-process cold save | Tested static-collision object pose and project-owned script JSON restored in a fresh process. |

These controls cover panel registration/phases, preferences/layouts, asset
browsing/publication, secondary-window routing and actual Scene/Game pixel
presentation. They are not all-platform or all-project certification. The
architecture source guard is supplementary: real callbacks, editor workflows
and packaged exports remain the behavior evidence. Combined human acceptance
and Windows V2 qualification remain pending. Retained legacy camera navigation
keeps its separately scoped regression evidence; this table does not certify
every first-focus/pan journey.

Before acceptance, use an existing project and a fresh project to check:

1. Create/select/rename/parent an object; edit Transform; Undo/Redo; save/reopen.
2. Import/find an asset, change folders/views, edit/attach a script and use the
   retained material/model tools. Hide the browser while a cook completes.
3. Play, focus Game, type in the source editor without moving the player, Stop,
   then debug/step/inspect/continue and deliberately reload a saved edit.
4. Dock/split/float Scene, Game, Inspector and Judas Tools; close/reopen panels;
   save a named layout, restart, restore it and reset. Test light/dark and scale.
5. On an admitted native backend, detach a view/tool and verify rendering,
   keyboard/mouse ownership, closing and layout recovery. Record backend and
   monitor/DPI setup; do not infer other platforms from this result.
6. Export and run a relocated standalone. Editor layout changes must not alter
   its game scripts, physical state, packaged assets or runtime UI.

Pending human review is not inferred from a screenshot, synthetic input or a
successful automated build. Main and published Alpha packages remain unchanged
until separately authorized.

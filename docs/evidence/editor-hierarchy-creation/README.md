# Hierarchy object creation — 2026-10-25

Operator request: create objects directly from a hierarchy right-click, with
child creation on existing rows and root creation in empty space. New objects
must be selected immediately and have ordinary useful defaults.

Implementation:

- Empty hierarchy/list space offers the existing 15 object types directly.
- Existing row menus offer **Create child** and forward that row's exact ID.
- Main Create, row and empty-space menus share one implementation in
  `EditorCreationMenu.cpp`; their catalog cannot drift independently.
- Root objects start at the scene-view focus. Children start at local origin,
  identity rotation and unit scale, inheriting their parent's frame normally.
- The existing creation function was moved unchanged in substance from
  EditorPanels.cpp to the creation module, then extended with a checked optional
  parent. Its normal component defaults, immediate selection and single Undo
  transaction remain intact. A stale parent fails before mutation.
- No scene creation is offered during Play. Empty-space menu Escape cancels
  explicitly, including when keyboard navigation is not configured.

Focused validation: **85 checks, zero failures**. Actual ImGui right-click/menu
activation covers every root object type, one-shot commands, placement, parent
request forwarding, Escape and Play exclusion. Real document creation covers
parent/local transform, sole selection, dynamic-body/render/mass defaults,
single-step Undo/Redo, root/static defaults and stale-parent rejection.
The row test exercises the shared row-menu contents with a supplied parent;
the complete hierarchy row interaction still needs operator acceptance.

Reproduce:

```sh
cmake -S . -B build
cmake --build build --target judas_editor_creation_menu_tests judas_editor -j 2
build/judas_editor_creation_menu_tests
sha256sum -c docs/evidence/editor-hierarchy-creation/SOURCE.sha256
```

Human check: reopen the rebuilt editor after saving current work; right-click
empty hierarchy space and create a static box. Confirm selection/inspector, Undo
and Redo. Right-click that box and create a child sphere. Confirm nesting,
parent-local transform and selection. Create a light and camera the same way;
verify creation is unavailable during Play. Mesh creation retains its existing
asset-selection workflow; it does not generate a mesh or invent an asset.

Experimental follow-up only; no main promotion or human acceptance claimed.
No operator project or existing editor session was modified or closed.

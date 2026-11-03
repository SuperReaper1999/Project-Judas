# Satchel Menu Workshop

An editable inventory/pause menu demonstrating native drag requests and a live
3D equipment preview. Experimental only; requires the matching engine build.

Open `Satchel_Menu_Workshop.judasproj` in the editor and press Play, or run the
standalone package produced for this pass. Escape resumes/reopens the menu.
Drag bag/chest/equipment slots, or focus and Activate. Use the visible action
buttons for consuming, equipping, splitting, transferring, saving and loading.
Left/right rotates the mannequin. Inventory, Character and Settings tabs work.

`Assets/items.js` defines items; `menu.js` owns inventory/equipment rules,
UI policy and saves. `Assets/satchel.judasui` contains the authored layout. The floor and
falling sphere demonstrate that the modal menu really pauses physical gameplay.
No controller code is taken from or installed into your RPG project.

Original art generation: `tools/demos/menu_workshop/art.py` (Pillow, authoring only).
Native authoring: `judas_menu_workshop SOURCE_ART NEW_PROJECT_DIRECTORY`.
Export through the ordinary editor or `judas_export` workflow. All referenced
icons are registered runtime assets rather than untracked string dependencies.

See [engine guide](../../docs/UI_TRANSFERS_AND_PREVIEWS.md) for APIs and boundaries.
The mannequin is render-only; normal scene lighting is shared. This example targets
desktop/landscape and does not promise portrait reflow or physical-device testing.

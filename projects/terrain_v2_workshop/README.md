# Terrain V2 — Highlands Workshop

Editable Experimental terrain game. The sampled landscape and local excavated
surfaces share one physical source. The uncut terrain collider is replaced;
there is no separate cave prop hiding in front of an intact hillside.

Run `build/judas projects/terrain_v2_workshop/terrain_v2_workshop.judasproj` or open
that project in `build/judas_editor`. In the editor, use World → Terrain tools,
Edit landscape, Select landscape and the viewport brush. Save and update publishes
source, preview and gameplay products; save the scene separately as usual.

- WASD and mouse: explore; Space: jump.
- T: excavate a tunnel progressively through the hill across chunk boundaries.
- Left click: dig where a real physics ray selects terrain.
- Right click: add rock at the selected surface. Digging is field editing, not
  scripted replacement meshes or disabled collision.
- 1/2/3/4 select dig/raise/lower/paint for left click; 5 cycles the four paint
  layers. Surface sculpting and paint survive saving alongside the tunnel.
- F6/F7: ordinary save/load; R: fresh scene; Escape: pause.

Wait for the tunnel queue to finish, walk through, save, quit and relaunch/load.
`Saves/` is player-owned and ignored by Git. The generator never deletes it.

Source: `Authoring/highlands.judasterrain2`. Preview/gameplay assets live in
`Assets/terrain`; the explorer is ordinary project JavaScript in `Assets/scripts`.
The scene's `terrain.asset` selects the native `.judasterrain` source. The engine
initializes its actual chunk collision before gameplay; the preview model is not
an intact collision mesh hiding underneath the excavation. Saves retain the base
asset identity and edit journal. If you change the authored base after saving,
start fresh or provide an explicit content migration.
`tools/create_terrain_v2_workshop.py` regenerates only this owned example and its
original procedural material textures. It requires `build/judas_terrain_author`.

This remains an Experimental preview requiring human review. Native authoring,
regional streaming, source-owned navigation, planetary integration in the separate
Planetary Workshop, and packaged-SDK/relocated exports are now exercised. Runtime
sources still load all samples and collision chunks while resident; per-chunk
physical interest and demand paging remain open. Advanced Fluids and release
policy are not changed by this project.

The detailed grass, earth, rock and sand textures reuse the existing Lunar Island
CC0 material catalogue. Sources and notices are preserved in
`Assets/terrain/REUSED_MATERIAL_NOTICES.txt`. The original generated fallback
textures remain as independent assets. A small ordinary preview atlas makes
terrain readable before its editing tools are opened; runtime and brush previews
use the full tiled layers rather than that atlas.

The landscape now opts into source-owned live navigation using its existing
Navigation Surface component. The local 50×24×50 m coverage uses 0.25 m cells.
Navigation follows terrain edits and saves with the actual collision; the HUD
reports readiness. Other scene props would need navigation obstacles or separate
surfaces. This scene's changed configuration changes its save compatibility
identity; older Workshop saves are retained, not silently migrated or deleted.

Brushes queued by T are ordinary persistent project state. Saving midway through
excavation preserves completed terrain and the remaining queue; a cold load
resumes accepted work without losing or repeating a dab. The qualification driver
verifies this before walking through the completed tunnel, rather than assuming
a fixed number of render frames is enough time for asynchronous workers.

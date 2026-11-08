# Terrain V2 — Planetary Workshop

An editable Experimental project using the normal native-terrain component,
planetary rendering/collision, radial gravity, CharacterMotor and JudasJS.
The hill starts intact. Excavation changes real collision; the underlying
planet is removed over the geographic patch rather than secretly blocking it.

Open `terrain_v2_planet_workshop.judasproj` in the matching Experimental editor,
then Play. Or pass the project file to the matching `judas` executable.

- WASD/mouse: walk/look; Space: jump.
- T: cut a tunnel through the hill. Wait for the displayed cut queue, then walk
  through it. Cuts publish between physics steps without exposing half a mesh.
- 1/2/3/4: excavate/raise/lower/paint; 5: choose material.
- Left click: use the selected tool; right click: add material.
- O: request a normal world-origin shift around the player.
- F6/F7: save/load the `planetary` slot. Quit/relaunch/F7 to test cold persistence.
- Escape: pause; R: reload the authored scene. Existing saves remain available.

In Edit mode, select the landscape and open World → Terrain tools. Sculpt,
paint or excavate, then **Save and update playable terrain**, followed by normal
scene Save. Source undo and scene undo have their existing distinct scopes.
Play-time digging is saved gameplay state; it does not rewrite authored assets.

To create another patch, use Create Terrain V2 landscape → Landscape type →
Planetary patch, choose its planet and centre the scene view on the desired area.
The physical rim determines sample pitch. The outer rim is protected, patches
cannot overlap, and footprints must remain inside one cube face. Changing the
parent's surface provider/collision spacing requires regenerating affected
patches; a stale source rejects Play rather than silently disconnecting.

`Authoring/planet.json` contains the parent descriptor. The editable source is
`Authoring/highlands.judasterrain2`; cooked native/preview assets are under
`Assets/terrain`. `Assets/scripts/explorer.js` owns the game controls. Its pending
brush positions use absolute coordinates so origin shifts do not move the cuts.
`geography.js` contains the authored demonstration route, not an engine solver.

The generator `tools/create_terrain_v2_planet_workshop.py` accepts `--output NEW_DIR`
and refuses existing directories. It uses the engine's `v2-planet`/`v2-native`
commands and retains the existing texture/font licence notices. No downloaded
assets or external authoring services are required.

This is a bounded local excavation region on a planet. All 64 native collision
chunks remain resident while active; their visual meshes use native detail.
The rest of the planet retains its existing LOD and collision-interest system.
This is not arbitrary planet-wide volumetric storage, cross-face excavation or
demand-paged native sample arrays. Alpha publication and human acceptance are
separate from automated engineering checks.

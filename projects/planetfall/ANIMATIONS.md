# Player and animation status

The supplied Conner rig is used for the playable character and seated pilot. The source file remains outside this project and untouched. The game copy is `ArtSource/conner_planetfall.blend`; runtime uses `Assets/models/conner_player.glb`.

Source SHA-256: `8d0e81bfc94cdf8d6626cf1203db5bc9001d22e14a3b5af454051a43451d2316`.

The segmented rig has 21 joints and 43 skinned mesh pieces. The game copy has seven procedurally authored clips: Idle, Run, Jump, Punch, HeavyPunch, Shove and Seat. They are functional prototype clips, not polished motion capture. Zombies and pirates retain the proven consumer-game rigs and gameplay animation policies; deaths enter real ragdolls.

For hand polish, prioritize rifle idle/aim/fire/reload, landing and mantle transitions, seated steering, zombie attack/stagger and pirate strafe/fire. Open the game Blender copy, animate the existing armature without renaming joints, add an appropriately named NLA track, and export selected rig/meshes as GLB with skins and NLA animation tracks. Replace the game model through the normal asset path while preserving its `.judasmeta` identity. Check third-person playback and both camera modes afterward. Do not overwrite the supplied original.

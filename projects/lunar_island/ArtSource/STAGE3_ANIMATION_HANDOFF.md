# Stage 3 animation work for a human artist

Current assets are deliberately usable foundations, not polished combat animation.
The player model is the existing original adventurer; survivor/bandit variants use
the same rig. Wolf output is currently a static mesh, not an animated character.

Priority clips:

- Wolf: walk/trot/run, bite windup/contact/recovery, flinch, death/resting pose.
  An armature and skin weights must first be authored; no existing wolf rig is implied.
- Humans: weapon-specific idle/aim/walk/run, pistol/rifle/shotgun firing and reload,
  knife/bat/tool attack, hit reaction and death/resting pose.
- Player: third-person hand/weapon placement and aim/reload presentation. Current
  first-person weapon recoil/swing is procedural; third-person hit logic works but
  does not yet provide held-weapon animation.

Practical Blender workflow:

1. Work on a copy of Assets/models/character_adventurer.glb or npc_survivor.glb.
   Import glTF into Blender, keep the armature/bone names and existing scale/rest
   pose. Keep the original source unchanged. The right hand is `hand.R`; arm names
   include `upper_arm.R` and `forearm.R`.
2. Author separate Actions. NPC policy currently requests exact names Idle, Walk,
   Run and Attack. Export glTF binary with skinning and those animation Actions,
   preserving names. Keep the original mesh proportions and test after reimport.
3. Put the result at the existing project model path, preserving its .judasmeta
   asset identity. Editor/runtime resources should be deliberately reloaded. The
   model replacements change content compatibility; use a disposable game save.
4. NPC Attack currently deals damage after a 0.45-second windup in person.js. Align
   the contact pose to that time, or deliberately change the game-side timing with
   the clip. Adding a named clip alone does not change combat rules or trigger it.
5. New weapon-specific states require updating person.js/player presentation, not
   just exporting more Actions. `Person.presentHeld()` demonstrates public resolved
   hand-joint attachment; do not bake game damage into a native animation system.
6. Rig the wolf separately. Enable its ordinary native animation component in
   author_stage3.py (animals currently skip it), and keep/update the requested clip
   names and +X animal facing convention. Retest motion, attacks and save reload.

For articulated death, use the engine's ordinary authored ragdoll workflow and
integrate it deliberately; current corpse prefabs are single dynamic proxies.
Do not present those proxies as completed ragdolls or simply add decorative limbs.

Run script/scene validation and actual encounter/cold-save tests after replacing
art or timing. Keep exported source .blend files and generation settings alongside
new original art when available. Current reproducible original rig/clip sources
are the preserved Hearthworld project and author_stage3.py.

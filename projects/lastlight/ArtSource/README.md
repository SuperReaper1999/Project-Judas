# Editable Lastlight combat art

Open these files in Blender 5.2.1 or newer. They are original project content,
not engine resources or changes to your backed-up starter pack.

- `combat.blend`: the same 17-joint adventurer rig used by the game, with Idle,
  Walk, Run, Jump, Punch, HeavyPunch and Shove actions; the fist/forearm mesh sits
  beside it. The combat clips are short visual poses without root travel.
- `zombie_motion.blend`: the same rig with ZombieIdle, Shamble, ZombieRun,
  Attack and Stagger, plus Idle/Walk/Run/Jump compatibility clips. These are
  simple in-place authored poses; see `Tools/zombie-motion.json`.
- `ordnance.blend`: launcher, rocket and grenade, each in its named collection
  and parented to an `<asset>_export_root` empty.

## Change a combat action

Select `Adventurer_Rig`, open the Dope Sheet in Action Editor mode, choose a clip,
and pose/key the bones. Keep existing bone names and hierarchy so current skins,
ragdoll mappings and game masks remain valid. Imported source actions use seconds
converted into the scene's 30 fps timeline. NLA tracks are muted to let the chosen
Action play on its own.

For GLB export, select the rig and `Adventurer_mesh`, use **Selected Objects**, enable
skins and animations, choose **Actions**, and export all wanted named clips with
**+Y Up**, one-frame sampling, each Action’s own interval and start slid to zero. Replace the corresponding `Assets/models/character_fighter.glb`; retain
its existing `.judasmeta` ID. Reload the project/scene to acquire the changed model.
No C++ recompilation is needed. Keep any new action name synchronized with JS.

## Change an ordnance model

Choose its collection and hide the other two. The empty's X position only spaces
models apart for editing. Before exporting that collection's root and meshes,
set the empty's location to `(0,0,0)`; restore its display location afterward.
Export **Selected Objects**, GLB, **+Y Up**, materials enabled, animations disabled.
Use the matching filename under `Assets/models/` and keep its `.judasmeta`.
The launcher/rocket point along glTF local +Z; the game turns that forward into
the camera's -Z direction. The launcher pivot is its rear hand grip.

## Remaining presentation work

The game currently uses a rigid rifle/launcher visual with script recoil and gun
lowering/tilt during reload. It has no dedicated two-hand grip, magazine-change,
RPG shoulder-fire or grenade-throw animation. Those can be authored in this rig
and selected/layered by project JS later. Punch/heavy/shove are already genuine
skeletal clips. Hit timing, damage and physical reactions remain game/physics
logic rather than being encoded in the animation asset.

See [the complete animation handoff](../ANIMATION_GUIDE.md) for clip selection,
root-motion ownership, export settings and remaining presentation gaps.

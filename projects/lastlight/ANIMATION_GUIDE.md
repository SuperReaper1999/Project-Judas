# Lastlight animation handoff

Lastlight's animation assets are editable original project content. The original
starter pack in `LowPolyAssets/low_poly_starter/` remains unchanged. You do not need
to rebuild Judas C++ to change a model or select a new animation clip.

## Files to open

| Blender source | Runtime model / purpose |
|---|---|
| [ArtSource/combat.blend](ArtSource/combat.blend) | `Assets/models/character_fighter.glb`: player skeleton with Idle, Walk, Run, Jump, Punch, HeavyPunch, Shove; editable FPS fist/forearm beside the character. |
| [ArtSource/zombie_motion.blend](ArtSource/zombie_motion.blend) | `Assets/models/zombie_animation.glb`: ZombieIdle, Shamble, ZombieRun, Attack, Stagger, plus Idle/Walk/Run/Jump compatibility clips. |
| [ArtSource/ordnance.blend](ArtSource/ordnance.blend) | `Assets/models/rocket_launcher.glb`, `rocket.glb`, `grenade.glb`: editable static geometry/materials. |

The manifests in `Tools/combat-models.json`, `Tools/zombie-motion.json` and
`Tools/ordnance-models.json` record the exported clips and asset identity. The
corresponding `Tools/build_*.py` scripts reproduce the original authored content;
the combat/zombie generators use the local preserved starter-pack authoring
helpers. Running those generators again overwrites the generated outputs, so
keep a separate copy of your manually edited Blender source first.

## What is finished, and what is still placeholder

- Punch, HeavyPunch and Shove are actual skeletal clips, with project JS deciding
  their hit timing. The FPS fists are separate low-poly visual meshes.
- Zombies have arms-out idle, shamble and pursuit poses. Their `Attack` clip is a
  right-handed punch with a visible windup, extension and recovery. These are
  simple in-place clips, not motion capture; gameplay variants share the same
  visual rig. There is no foot-plant IK or dedicated bite, vault, death or
  getting-up clip. Death uses the existing passive ragdoll, not a death Action.
- Rifle/launcher placement is currently script-authored. Recoil, sway, aiming and
  reload lowering/tilt move the weapon visual. **That is not an animation of hands
  exchanging a magazine.**
- Dedicated two-hand rifle/RPG grips, magazine-change, shoulder-fire and grenade
  throw clips remain presentation work. Fingers use a stylized mitten shape.
- Physical trips are passive ragdoll motion. Returning to animation captures and
  fades the fallen pose; it is not a hand-authored stand-up/recovery sequence.

## Edit an Action in Blender 5.2.1

1. Open the relevant `.blend`. Select `Adventurer_Rig`, then switch to **Pose Mode**.
2. Open the **Dope Sheet**, choose **Action Editor**, and select the wanted Action.
   In `combat.blend`, imported NLA tracks are muted so the selected Action can
   play alone. Retain that arrangement unless intentionally editing NLA.
3. Move the timeline, pose the bones, and insert rotation/location keyframes.
   The source scenes use **30 fps**; a 0.8-second Action spans 24 frame intervals.
4. For a new move, duplicate an existing Action before editing it and give it a
   clear unique name. Keep the Action retained with its fake-user/shield setting.
5. Preview the whole interval, including its first/last pose. Looping movement
   needs a matching seam. Keep attacks readable at the moment JS delivers the
   hit. Current player timings are in `Assets/scripts/melee.js`.

The zombie `Attack` lasts **0.9 seconds** at authored speed: windup through
0.243 s, fist extension at 0.441 s, contact pose through 0.576 s, then recovery.
`Assets/scripts/zombie_ai.js` owns the actual strike/reach checks and variant
timings; changing the Action does not change damage. Keep the arm extension and
the script's visible attack phase aligned when editing either. `ZombieIdle`,
`Shamble` and `ZombieRun` retain their original names and lengths, as do the
`Idle`/`Walk`/`Run`/`Jump` compatibility Actions.

Preserve the **17 deform-bone identities, parent hierarchy, bind/rest transforms,
vertex weights and inverse-bind relationship**. Names such as `pelvis`, `chest`,
`head`, `upper_arm.L` and `shin.R` are used by current ragdoll mappings. Renaming
or reparenting them requires deliberately updating those mappings too. Do not
apply arbitrary armature transforms or change the bind pose to fix one clip.

### Movement ownership

The CharacterMotor owns collision-aware root/world motion. The clips supply
joint-local visual poses. Keep locomotion **in place**: no forward-travelling root
track used as a replacement for the motor. Small pelvis/chest motion is visual;
it does not move the character's authoritative collision volume. Ragdoll physics
is another pose source through Judas's existing pose resolver.
Framewalk follows the motor orientation with the whole rendered skeleton, using
the existing Idle/Run/Jump clips. No unique wall walking animation or foot-plant
IK is implemented.

## Export an animated GLB

1. Select the rig and its skinned `Adventurer_mesh`; exclude the separate FPS
   fists, lights, cameras and unrelated props.
2. Choose **File → Export → glTF 2.0**, format **GLB**, and **Selected Objects**.
3. Enable **+Y Up**, skins and materials. Blender uses Z-up while the exported
   glTF/Judas model uses Y-up. The character faces glTF local **+Z**; the project
   rotates its presentation to face the view. This asset basis does not mean
   gravity is universally world-Y.
4. Enable animations with **Actions** mode. Export all wanted named Actions,
   use **Always Sample Animations** with a one-frame sampling step, and export
   each Action's own interval rather than forcing every clip to the scene's
   current frame range. Slide the exported animation start to zero. These match
   the project's generator settings.
5. Replace the matching GLB under `Assets/models/`, keeping its existing
   `.judasmeta` sidecar/ID. Reload the project/scene so its model resource is
   reacquired. Preserve a backup before replacing manually edited work.

A new independent asset needs a new ordinary asset registration. Do not reuse an
existing `.judasmeta` ID for an unrelated model. Merely editing the same model
should retain its identity.

## Select your clip in project JavaScript

The player selection is in `Assets/scripts/player.js`; melee clip names/timings
are in `Assets/scripts/melee.js`. Enemy selection is in
`Assets/scripts/enemy.js`. The runtime clip list is available through
`entity.animation.info.clips` or `entity.animation.clips` after `info.ready`.

The existing API selects a clip by its exported Action name:

```js
const animation = this.visual.animation;
if (animation?.info.ready && this.currentClip !== "ZombieRun") {
    animation.loop = true;
    animation.crossFade("ZombieRun", 0.12);
    this.currentClip = "ZombieRun";
}
```

For a newly exported Action, substitute its exact name and choose whether it
loops. Trigger selection **when the desired clip changes**, not every frame.
Rapidly interrupting fades can exhaust the resolver's bounded contributor budget;
Lastlight's enemy animation selection uses dwell/hysteresis and isolates visual
failures so animation cannot disable combat behaviour. A direct `play(name)` is
also available when an immediate replacement is appropriate.

Do not encode damage, ammunition, navigation or root movement in the Blender
clip. Those remain project JS consuming ordinary engine primitives. See the
[JudasJS animation reference](../../docs/judasjs/animation-ragdolls.md).

## Static launcher / grenade / rocket edits

Each ordnance collection has an `<asset>_export_root` empty. Its X translation
spaces the source models apart for editing. Hide other collections, edit the
wanted mesh/material, then set that root location to **(0,0,0)** before exporting
its root and meshes. Restore its display offset afterward.

Export GLB, **Selected Objects**, **+Y Up**, materials on, animations off. Retain
the matching sidecar ID. Launcher/rocket forward is glTF **+Z**, and the launcher
pivot is its rear hand grip; current weapon presentation rotates it toward the
camera's -Z direction. The grenade's authored pivot is near its bottom, so keep
its visual offset consistent with the projectile's collision shape.

[ArtSource quick reference](ArtSource/README.md) · [Game controls](README.md)

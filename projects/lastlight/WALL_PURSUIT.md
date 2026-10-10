# Zombie wall pursuit

Project gameplay revision, 10 October 2026. Zombies can make a finite jump toward
a nearby physical wall, change their own gravity reference, and chase the player
along it. No new engine or JudasJS binding is involved.

## Behaviour

`zombie_ai.js` remembers an observed player position and surface up. Visibility
uses each character's own eye orientation and actual three-dimensional range.
Sound still provides a search point rather than live knowledge through walls.
`zombie_framewalk.js` probes nearby real static/kinematic geometry, checks the
configured capsule's landing space and approach, and supplies ordinary jump and
movement intent. A grounded jump leaves support before the gravity turn starts.

Gravity blends through the existing `entity.gravity` API; CharacterMotor owns
orientation, collision and displacement. No pursuit transform is teleported.
Ground navigation remains the original Detour route. It pauses during local
surface pursuit; remembered surface-tangent guidance drives the same motor there.
Wall references retain local points/normals and safe entity handles. Probes are
staggered, limited to 4.8 m and never enumerate the entire scene.

Facing and the rendered skeleton follow the motor's authoritative/presented
orientation, with yaw relative to the model's local axis. Punches wait until a
frame turn finishes. Locked lunge motion is projected into the current tangent
plane before combining retained normal velocity, avoiding repeated acceleration
when the up axis changes. Near-goal braking reduces running through the player.

Returning to reachable ground, losing the surface or forgetting the target
restores prior gravity selection. A high target on a roof does not cause repeated
floor/wall switching merely because the roof shares ordinary up. Death or a trip
captures the current oriented pose, then releases the wall reference; passive
bodies fall under normal physics. Living trip recovery uses spatial gravity and
a matching standing capsule, with the existing captured-pose return fade.

## Try it

1. Collect and equip the boots through C / I.
2. During a wave, jump toward a nearby building wall and press Space again while
   airborne. Let a zombie see or hear you near that wall.
3. Move upward and watch it jump, turn and follow. Shoot it on the wall: its real
   ragdoll should fall away.
4. Press X and drop to the street. Watch reachable pursuers return to ordinary
   ground movement; pause/restart still use the existing game UI.

## Limits and review

This is bounded local surface pursuit, **not** a wall/ceiling navigation bake or
an arbitrary three-dimensional route planner. Occluded, distant, cramped or
disconnected faces can be unreachable; a creature may search, lose the trail or
fall back to ordinary gravity. A roof maze is not guaranteed traversable. Motor
instances still lack mutual physical blocking; project braking helps but does
not provide a new collision capability. No procedural climbing/IK animation was
added: the current run/punch skeleton is oriented onto the surface.

Actual application previews and the genuine transition failure/follow-up are in
`Review/wall-pursuit/`. Disposable fixtures arrange nearby cases using public
APIs; they are not shipped gameplay or a normal boots-collection proof.
Human gameplay acceptance of this new behaviour remains pending.

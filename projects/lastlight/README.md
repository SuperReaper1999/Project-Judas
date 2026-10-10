# Lastlight — town defence

An ordinary JudasJS game using your original stylized low-poly assets. Survive
**five zombie waves** across a 96 × 108 m neighbourhood with **eight enterable
houses**, connected rooms, front/rear exits, open windows and back streets, plus
**three tall Framewalk buildings** with real interiors, ceilings and roof routes.
Move crates into doorways to buy time, watch the rear entrances, and purchase
explosives at the blue supply stall near the starting area. Collect the optional
**Judas Framewalk Boots** to change which physical surface acts as your floor.
Boots begin unequipped; ordinary gravity and movement remain the starting mode.
There are no hostile
soldiers in the active game; their original unused prefab is retained.

## Play

Open `lastlight.judasproj` in Judas Editor and press Play, or launch the current
Judas runtime with this project. The Linux standalone is generated at
`.cache/Lastlight-Package/` in the repository; run its `judas` executable.
The package can be moved elsewhere and launched from another working directory.

| Control | Action |
|---|---|
| WASD / mouse | Move / look |
| Left mouse | Fire; hold for automatic rifle fire |
| Left Ctrl | Aim; reduced spread, view zoom and look sensitivity |
| 1 / 2 | Rifle / RPG launcher |
| R | Reload the 24-round rifle; reserve rifle ammunition is unlimited |
| G | Throw a purchased grenade; 2.4-second fuse |
| N near the blue stall | Open the points vendor |
| I | Open/close the inventory; select weapons, equip/unequip boots or drop one item |
| C while aiming at a nearby item | Collect into inventory; boots remain unequipped |
| B while aiming at a nearby crate/barrel | Pick up / release |
| Look down while carrying | Drag along the ground |
| T while carrying | Turn the prop by a quarter turn |
| Right mouse / F / Q | Quick punch / heavy punch / shove |
| Space / Left Shift | Jump / move faster |
| Second Space press after jumping, with boots equipped | Select an aimed clear wall, ceiling or floor within 7 m; changes gravity without another jump impulse |
| X | Release Framewalk and restore prior gravity; boots remain equipped |
| E | Mantle a reachable short ledge/hurdle |
| V | Switch first/third person, keeping the same character |
| Escape | Pause/resume and release/capture the pointer |
| Pause menu | Restart the scene/round or quit |

Controller bindings exist for move/look, trigger fire/aim, jump, mantle, view and
pause. Extra equipment, melee and crate controls currently use keyboard/mouse.
No controller hardware review or Windows run is claimed for this game.

## Fortifying a house

Aim at a crate/barrel within 3.2 m and press B. It remains a dynamic body, held by
bounded force/torque; walls, floors, other props and zombies still collide with it.
The holder's motor ignores only that held body until release, preventing the grip
from pushing its own holder. No parenting, collider removal or body teleport is
used. Carry speed is reduced and weapons/mantling are unavailable while holding.
Look down to drag, turn gently, and press B to drop in a doorway. Released props
retain real motion and can be pushed/broken by zombies; a loose single crate is
a temporary obstacle, not an invulnerable fixed barricade. Stack/position several
and remember the back door. The motor's normal body collision is restored on drop.
While Framewalk is active, the held body's gravity explicitly follows the
player's current acceleration. Releasing it or releasing Framewalk restores that
body's saved uniform override, selected gravity source, or normal spatial gravity;
an unavailable old source falls back to spatial gravity. The force grip still owns
no transform or velocity reset, and the prop continues colliding with the world.

## Inventory and Framewalk route

Aim at the boots on the low plinth just to the right of the start and press **C**.
Press **I**, select **Judas Framewalk Boots**, choose **Equip**, then close the pack.
Jump from a supported surface, release Space, aim at a nearby wall/ceiling/floor
and press **Space again while airborne**. The second press chooses a real physical
reference; acceleration blends toward the new floor while world momentum
continues. Opposite gravity directions briefly pass through zero; the motor
retains its up there and turns at its own authored rate. The camera and whole
player skeleton follow the presented motor pose.
Press **X** or unequip the boots to restore prior gravity: the town's normal
spatial gravity by default, or a saved uniform override/live selected field when
reusing the boots. A vanished source falls back to spatial gravity. A fresh
supported jump arms another airborne selection; stepping off a ledge alone does
not arm it.

The amber ground markers lead east to the **workshop, tower and depot** at x=37,
with roofs 7.4 / 10 / 8.5 m high. Explore their wide entrances, breakable wooden
doors, inner climbing columns, upper galleries, open windows and roof openings.
Two canopies link the buildings. Grenade/rocket supply cases on the roofs reward
traversal and feed the same stock used by the vendor and weapons. Floors, walls,
ceilings and canopies are ordinary collider geometry; entering a building does
not automatically change gravity.

The pack has **eight stored stack slots**, initially occupied by the owned rifle
and RPG. Boots use the `feet` equipment slot. The starting guns stay owned and
cannot be dropped; selecting them uses the same weapon selection as 1/2. Rockets
and grenades appear as shared supply stacks capped at 12 each, outside those eight
pack slots. Purchases, collection, firing and drops all use the original stock
counters. Collection checks a solid cast from the actual eye within 3.2 m and
requires room for the whole pickup. A blocked or full pickup stays in the world.
The inventory pauses the round; **Drop** closes it and, on the resumed physics
step, spawns one ordinary inert pickup if the space ahead is clear. Dropping an
equipped pair also unequips it. Restart rebuilds starting inventory, equipment,
supplies and world pickups; no cross-scene inventory workflow is implemented.

Active boots attempt nearby recovery if their reference disappears or no clear
landing remains in their current gravity direction. This searches bounded nearby
geometry and checks visibility, landing room and approach clearance. It can miss
a usable route or reject a cramped one; failure restores prior gravity, with
spatial fallback if its selected source has vanished.
**Fall damage still applies**, including preparation time: impact above 9 m/s
relative to the support's velocity, into its normal, deals eight health per excess
m/s, rounded up and capped at 100. Changing floors or pressing X does not erase
falling momentum.
See [FRAMEWALK.md](FRAMEWALK.md) for configuration, ownership and recovery limits.

## Combat, waves and points

- Waves contain **8 / 10 / 12 / 14 / 16** zombies. Initial preparation is 18 seconds;
  subsequent breathers are 12 seconds and restore up to 25 health.
- Runners are quick (70 health, 100 points), brutes are slower/tougher (140 health,
  175 points), stalkers flank (85 health, 125 points). Stalkers approach a sampled
  side/back route; enemies use sight, hearing, last-known-position search, visible
  attack windup and motor-resolved lunges. They push/bash destructible obstructions.
  Their pursuit clips hold both arms out; the close attack is a visible punch,
  with variant-specific playback aligned to its scripted contact/reach check.
- Rifle: body damage 28, upright capsule head-height weak point 80, eight shots/s,
  reload 1.25 seconds. Aim reduces spread/recoil; movement adds sway. The camera
  chooses an intended point, then the shot casts from the player's eye, so an
  offset third-person camera does not grant shots through nearby cover.
- Score records points earned; **POINTS** is spendable currency. Start with 300.
  At the vendor: **two rockets 250**, **two grenades 150**, **up to 50 health 100**.
  Stock is capped at 12 of each explosive; a two-unit bundle is disabled if there
  is no room for both. Vendor UI pauses gameplay and releases pointer capture.
- Rockets are ordinary physical prefabs with an additional bounded M44 sweep
  along actual motion; a contact detonates, with a four-second safety fuse.
  Grenades are physical bouncing bodies with a 2.4-second fuse. Blasts use distance
  falloff, real collider surface points, cover queries and finite mass-scaled
  impulses. Ordinary dynamic objects need no gameplay tag to react; fallen
  ragdoll bodies react too. Solid environment/props stop
  damage; characters do not act as blast-proof walls. **Blasts can hurt you.**
- Punch/heavy/shove damage is 25/55/5. The nearest swept fist volume includes cover.
  Heavy attacks take longer to land. Shoving zombies into low geometry or off a
  ledge activates an actual 11-body passive ragdoll, bounded to six concurrent
  trips. Physics owns the fall; clear supported placement and a captured-pose
  fade return it to animation. Downed mapped bodies route hits to their owner.
- Death enters that same passive articulation from the current resolved pose,
  retaining the engine's observed motion and adding the killing impact. An
  already tripped zombie keeps its existing bodies. Corpses cannot score again;
  at most eight remain, with an 18-second lifetime. Wave completion preserves
  the last death rather than immediately erasing it. Restart cleans the world.
- Twenty-two movable breakable crate/barrel props begin in the scene. Crate/barrel health is
  60/90. Destruction creates six/ten real debris bodies, retaining total 18/24 kg
  mass and inherited motion. Debris expires after 12 seconds; max twelve groups.
  Props never award enemy score.
- Three wooden doors in the Framewalk buildings have 100 health and break into
  six real planks retaining their 24 kg total mass. Their debris uses the same
  bounded lifetime/group cleanup as other breakables.
- Mantle height is 0.35–1.55 m, using collision-aware motor motion and headroom
  checks. The delivery ledge near the start is 1.34 m high. This is simple mantle,
  not wall-running, hanging or a parkour framework.
- Restart reconstructs the scene, enemies, physical props, points, health and UI.

## Project and engine boundary

`Scenes/town.judas` owns ordinary geometry and a baked navigation surface.
`Assets/scripts/` separates player/director, camera, gunplay, melee, climb, force
pickup, inventory/equipment, optional Framewalk, vendor, explosives, enemy AI,
destructibles, combat ownership and HUD.
Prefabs contain independent zombies, original unused soldiers, physical ordnance,
props and debris. UI is ordinary authored `.judasui` content. All behaviour uses
public JudasJS APIs: CharacterMotor, logical input, casts/forces/impulses, prefab
construction, navigation guidance, animation, passive ragdolls, presentation poses,
signals, gravity selection, audio, particles, UI and scene reload.

**No engine source or binding was changed.** Navigation advises; the motor moves.
Camera/mesh following uses presented poses; shooting/physics uses authoritative
poses. Animation failures are isolated from damage. The human-reported immortal
enemy came from repeated interrupted Idle/Run fades faulting its entire script;
minimum dwell/hysteresis now lets contributors retire and a cosmetic fallback
preserves combat callbacks. Original failure log is retained under `Review/`.

Edit gameplay values in `weapon.js`, `melee.js`, `zombie_ai.js`, `player.js`,
`vendor.js`, `game.js`, `items.js`, `inventory.js`, `framewalk.js` or ordinary prefab
data; reload without recompiling C++.
`Tools/build_town.py` regenerates town/UI/prefabs with stable IDs; bake navigation
with the current `judas_navigation_bake` before export. Regeneration overwrites
hand-edited authored files: preserve your changes first.

## Animation handoff and limitations

See **[ANIMATION_GUIDE.md](ANIMATION_GUIDE.md)** for the missing clips and Blender
instructions. Editable rigs/models are in `ArtSource/`. The original asset pack
and backups are untouched. Current zombie clips improve the pose vocabulary and
turn/gait timing, but remain simple authored animation. Rifle/RPG placement,
recoil and reload tilt are procedural visual presentation; they are not finished
hand/magazine animation. Dedicated grip/reload, grenade throw, climb and
getting-up clips would improve it. No foot-plant IK is implemented.
Wall/ceiling orientation rotates the whole rendered skeleton; it uses the same
Idle/Run/Jump vocabulary, with no dedicated wall walking animation.

This is a modest original prototype, not a commercial FPS. No tactical squad AI,
living NPC system, arbitrary fracture or sophisticated weapon simulation. Motor
crowds can overlap despite avoidance. Loose barricades are attacked/pushed rather
than dynamically carving navigation. Trip recovery is not balanced physical
standing; deaths use bounded passive ragdolls, not a standing/recovery controller.
Blast cover is a single surface-point visibility test per candidate, not a
volumetric pressure simulation. The Linux game was human-reviewed before the
final requested death/punch/blast additions; those received focused application
checks. Controller hardware and Windows gameplay acceptance remain outstanding.
`Review/BUILD_NOTES.md` records current application captures and retains older
revision evidence. These checks do not replace your human gameplay review.

## Human checklist

1. Enter a house; explore both rooms and leave through its rear door.
2. Carry/drag/turn/drop crates into a doorway. Observe real collisions and zombies
   pushing/bashing the obstruction. Fortify multiple entrances.
3. Encounter fast runners, tougher brutes and flanking stalkers. Check readable
   windup, cover/search, damage and repeated kills; no immortal script-faulted enemy.
4. Aim with Ctrl, fire, reload and switch views. Check recoil/sway and head/body hits.
5. Use punch/heavy/shove; trip a zombie against low cover and shoot it while down.
   Confirm a kill leaves a physical corpse, including the last enemy of a wave;
   approach a living zombie and watch its arms-out pursuit and punch.
6. Buy rockets/grenades with N; check points decrease and kills earn more.
7. Use 2/LMB for RPG and G for grenades. Watch physical flight, blast feedback,
   cover, prop reaction and self-damage. Keep distance from explosives.
8. Mantle/jump, pause/resume and restart; check pointer capture and fresh state.
9. Collect the boots with C; check I shows them unequipped, equip and jump/press
   airborne Space toward a clear wall, then a ceiling/floor. Inspect the whole
   character in third person. Explore the east towers and collect roof supplies.
10. Check X/unequip restore ordinary gravity, clear drops transfer one item and
    blocked drops retain it, carried props inherit/restore gravity, and hard
    landings hurt.
11. Run the exported package after moving it.

Actual application captures and retained development records:
[Review/BUILD_NOTES.md](Review/BUILD_NOTES.md).

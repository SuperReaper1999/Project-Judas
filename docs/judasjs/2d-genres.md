# Platformers, action adventures and tile RPGs

Experimental v1.1.0 qualification notes. These are reusable primitives and examples,
not a built-in Mario, Zelda or Pokémon framework. Gameplay, damage, parties,
quests and progression remain project-owned JavaScript.

## Physical height, stacking and drawing

Use actual XY positions for platformer height. Native planar bodies must remain
coplanar: moving bodies along Z to change draw order can remove their physical
interaction. Draw order instead comes from sprite layer/order/Y sorting.

Eight native boxes under gravity and independently elevated solid platforms have
focused physical regression coverage. Existing actual framebuffer tests cover
layer order and two sprites crossing in Y. For top-down actors use a foot-origin
transform/pivot and matching collision placement; a sprite pivot alone does not
redefine the entity origin used for Y sorting. Roofs/bridges can use explicit
sorting layers and project-owned reveal logic. This does not create multiple
physical elevation floors automatically.

Current limits matter: static straight [one-way platforms](physics-2d.md#one-way-platforms)
are now available, but no built-in drop-through command, custom tile slope,
ladder/ledge/bridge-height controller or gameplay tile-edit API is supplied. Solid
platforms and native chains/convex shapes exist; specialised movement and
collision filtering need explicit project implementation. Don't call this a full
Mario movement package or fake height by changing a physical body's Z.

## Parallax backgrounds

Copy [parallax-2d.js](examples/parallax-2d.js) into project Assets and attach it to
a body-free root sprite. Select the actor followed by your camera as `target`.
Keep camera-relative offset constant. Set factorX/factorY in the Inspector:
0 follows the actor completely (screen-locked); 1 stays at its authored world
position; intermediate factors make distant backgrounds scroll more slowly.
Put backgrounds on negative sorting layers; size/repeat scenery to cover the
travel area. The example does not automatically repeat textures or spawn tiles.

The example follows interpolated presentation, retains original anchors across
saves, and rejects physical/parented owners. Its fixture verifies independent
axis factors and repeated presentation without drift using the real runtime.
It is **single-camera, flat XY scene policy**: no native per-camera parallax
component, live origin-shift anchor correction or independent split-screen
parallax is promised. Do not put physics or gameplay hitboxes on these layers.

## Sword stabs and swipes

Copy [melee-2d.js](examples/melee-2d.js) into project Assets and import `swordHits`.
Call it on the completed active animation marker, rather than every rendered
frame. Sprite animation events already provide the fixed-boundary marker workflow.

```js
for (const marker of this.entity.spriteAnimation.events) {
  if (marker.name !== 'strike') continue;
  const options = this.state.attack === 'swipe'
    ? {reach:1.4, radius:.12, arc:Math.PI/2, samples:17}
    : {reach:1.6, radius:.1};
  for (const hit of swordHits(this.entity, this.state.aim, options)) {
    if (hit.entity.hasTag('damageable')) {
      signals.send(hit.entity, 'game.damage', {amount:1});
    }
  }
}
```

Import `signals` from `judas`. Configure the `damageable` project tag. Targets
subscribe in start/restore and own health/defeat in saved script state. Aim is a
nonzero local XY vector `{x,y,z:0}`; the helper rotates it into the owner's fixed
physical plane. Thrust uses one circle cast. Swipe uses at most 17 casts in an arc,
returns each encountered entity once and does not skip intervening walls. Native
fixtures prove target identity, multiple hits, deduplication and wall occlusion.
Filtering teams, damage, cooldowns and interaction priorities remain game policy.

This is a **sampled fan**, not an exact continuous rotating-sword sweep. Very small
targets between sample rays can be missed; use suitable radii/spacing, or author
an active-frame sensor workflow for your combat requirements. Multiple active
markers need per-swing hit-ID state to avoid repeat damage. There is not yet a
native multi-hit planar overlap API. These boundaries are release documentation,
not claims that an unsupplied combat game has been human tested.

## Pokémon-style top-down RPGs

Available foundations: painted layered maps with real collision; animated actors;
logical input actions; native queries/triggers; ordinary scene transitions;
project UI text, lists, focus and menus; script properties/signals; session state;
and modern cold saves. Beacon Grove composes triggers, objective state, menus and
physical movement. The tilemap workshop composes map changes and saves.

An editable [grid-step preset](physics-2d.md#grid-rpg-preset) is now available.
NPC schedules, encounters, dialogue branching, party/storage/inventory rules and
turn-based battles remain project scripts/data. There is no advertised Pokémon-style authoring/battle framework.
Do not assume native 3D navigation bakes a 2D tile path: use a project-owned grid
pathfinder for a bounded tile RPG. Saves support authored script state, but content
updates still require deliberate compatibility/migration policy.

A Doom source port is separate work: 2.5D renderer/world semantics and upstream
code/content licensing require their own investigation, not these 2D acceptance
checks. No port or compatibility claim is included here.

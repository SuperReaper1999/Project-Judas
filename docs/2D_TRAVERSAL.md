# One-way platforms and grid movement

Experimental follow-up to ac14d677430bf7fd78d12fc3aa61789afd0208f4. No main
promotion, release publication or human/platform acceptance is implied.

## Behaviour

- Optional immutable planar `oneWay` geometry: straight static two-vertex chains
  block on their authored left side. Left-to-right is a platform blocking from
  above. Existing two-sided geometry retains its exact version-1 encoding;
  opted-in geometry uses descriptor version 2. Scene/prefab/modern persistence
  uses the existing owners rather than a parallel platform subsystem.
- Response/CCD eligibility uses support geometry at the start of a fixed step,
  within the existing native 2 mm minimum contact margin. Broadphase pairs remain
  independent of transient eligibility. Core distance geometry remains continuous
  during CCD. No global collider enable/disable, extra timer, volume/velocity
  repair, solver-budget increase or private save cache was introduced.
- Dynamic and kinematic non-sensor observations respect the blocking side;
  explicit sensors still report geometric overlap. Casts use their current start
  side. Simultaneous upward/downward actors make independent decisions.
- Editor Create provides a visible **2D one-way platform**. Body → Native 2D
  exposes the option for straight chains; collision preview marks the blocking
  normal. Fixed rotation changes its gravity frame naturally.
- **New 2D game - grid RPG** and **2D grid RPG player** provide an editable
  project-owned script with native kinematic box movement and collision queries.
  Settings expose cell dimensions, speed and camera ownership. Collision/facing,
  committed cell, pending target and a single buffered turn are ordinary saved
  script state. Motion uses velocity integration; no corrective teleports/snaps.
- Directional button actions preserve very short taps; axes remain available for
  held/analogue movement. Presets preserve user-configured bindings and add missing
  grid_left/right/up/down defaults. The hierarchy command persists those project
  actions. Invalid existing action types reject with a useful error.

Detailed authoring, API fields and scope:
[Native 2D physics](judasjs/physics-2d.md#one-way-platforms) and
[grid preset](judasjs/physics-2d.md#grid-rpg-preset).

## Human checklist

Open the editable **2D Traversal Workshop** project (projects/2d_traversal_workshop).

1. Scene 1: walk under a gold platform, jump through it, land and stand on top.
   Walk off and jump again. Inspect its blocking arrow in Body collision preview.
2. Press 2: tap cardinal directions, release before a cell finishes, hold to repeat,
   and try diagonal input. Green solid blocks must block full-cell steps.
3. Save/load during a step; repeat after reopening the standalone.
4. In the editor, create a one-way platform and a grid RPG player. Keep one camera
   owner; disable the prior player script or the new controller's followCamera.
5. Try New Project → New 2D game - grid RPG; save/reopen and Play/Stop.

## Explicit boundaries

Static straight one-way segments only: no moving/curved one-way platform,
automatic one-way tilemap layer or built-in drop-through command. Deeply
intersecting spawns are not repositioned above a surface. Put one-way artwork in
a non-solid tile layer and author these segments separately.

Grid movement is a single-player, fixed-frame project preset, not dynamic
CharacterMotor/ragdoll movement, a pathfinder, party/battle system or live origin
rebase adapter. Native kinematic interactions retain their normal authority.
A blocker can delay a step; reverse returns to the last committed cell.
Multi-camera/repeating parallax and content-specific RPG/combat systems retain
their separately documented limits. This is not an Alpha release decision.

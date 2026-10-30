# One-way platforms and the grid RPG preset

Experimental follow-up to ac14d677430bf7fd78d12fc3aa61789afd0208f4. No main
promotion, release publication or human/platform acceptance is implied.

## What changed

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

## Real failures found in the new controller

An obstacle inserted into an accepted step could overlap the actor, trapping the
back-out command in an initial-overlap cast. The corrected controller allows
separating motion but repeats the cast with that overlap ignored, so a second
blocker behind it still stops the actor. The bounded eight-query exhaustion is a
conservative stop, never invented clear passage. Tests cover both cases, and a
returned partial step does not increment completed-cell count.

The axis-only prototype also lost a direction pressed and released before the
next fixed step. An unchanged native fixture using that preserved source fails
its short-tap assertion. The corrected project bindings/controller pass it and
the buffered-turn test. This is a preset-policy correction, not a change to the
engine's input timing or physics clock.

## Export-tour defects caught before checkpointing

The first combined scene inherited the grid starter's interior obstacle, blocking
its intended jump route. The corrected platform scene removes that unrelated
obstacle. This was a content-composition defect, not a solver failure; the
original tour log and source project are retained locally.

The tour also established a generic harness bug: ENTITY telemetry combined an
authored scene position/orientation with live physical velocity. It now samples
the current native pose at alpha 1, matching the existing Entity transform path.
An independent verifier rejects the original log and passes 175 assertions over
the corrected 55-frame observation set, including both platform landing heights
and blocked/returned grid steps. It deliberately compares live body fields, not
cached controller flags. No physical trajectory is changed by the telemetry fix.

## Verification

Focused Linux native checks include:

- Upward crossing then landing/resting; unchanged two-sided negative control;
  120 m/s downward CCD; rotated plane/gravity; two actors on the same platform;
  support queries; nonblocking kinematic events and explicit sensor overlap.
- Versioned descriptor roundtrip, malformed flag atomic rejection, unsupported
  shape rejection, ordinary prefab placement and normal scene serialization.
- Mid-crossing real modern save, independent restoration with identical physical
  endpoint/velocity, and a separate fresh-process continuation.
- Single cell, held/diagonal inputs, blocked destination, inserted obstruction,
  safe reversal, second-blocker negative control, sub-frame tap and buffered turn.
  Actual mid-cell snapshots resume correctly in a fresh process.
- Hierarchy grid-player construction, source attachment, native motion authority
  and script startup, plus original editor/script workflows.
- Existing planar physics including the fixed 3D trajectory digest, MK2 physics
  and rotational energy regressions, tilemap/prefab integration and Beacon Grove.
- Real editor New Project grid-template handling, save and Play/Stop; relocated
  standalone traversal scenes with collision, scene changes and save/load.

[Evidence and reproduction commands](evidence/2d-traversal/README.md) record exact source/binary fingerprints and separates preliminary
setup/prototype failures from final results. Builds use two compiler jobs on the
8 GB Linux host. Windows/Android and Ubuntu-compatible SDK packaging are not qualified by this
host-build pass. The relocated export is a local test package, not a release artifact. These are
focused regressions, not a universal platformer/RPG feature certification.

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

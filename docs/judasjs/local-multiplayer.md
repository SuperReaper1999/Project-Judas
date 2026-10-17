# Local multiplayer — `players`, `LocalPlayer`, `PlayerInput` (M89)

Optional, world-owned local identities, device routes, viewports and UI focus.
There is **one authoritative world and fixed-step clock**, regardless of view or
player count. No networking, replication, prediction, server authority or game
rules are implied. [Input](input.md), [physics/CharacterMotor](character.md),
[cameras](effects-camera.md), [UI](ui.md), [scene/save lifetimes](scenes-state.md).
M99 adds a separate optional [online session layer](network.md) which can associate
remote peers with these same local-world player identities; this namespace remains
usable independently of networking.

## Identity and lifetime

`players.create(name='Player', map=null)` creates an identity (maximum eight).
Name is at most 128 UTF-8 bytes. Identity creation also works without a Window
or device adapter. `map=null` copies the current input map (headless: engine defaults);
otherwise supply normal named input entries (maximum 256 logical entries).
`players.list` returns current handles; `players.get(id)` returns null if absent.
IDs are decimal strings, process-monotonic and never recycled. `player.valid`
becomes false after leave/world teardown; other stale operations throw ReferenceError.
Player and input handle constructors do not create identities.

`player.entity` is an optional association with an ordinary entity. Setting it
neither grants exclusive write authority nor changes physics, parenting, camera,
input, animation, streaming interest or ownership. The entity may be a motor, vehicle, rigid body or
anything else. Destruction makes this getter null; the player remains. Changing
an entity/device/camera does not replace the player. Projects decide control policy.

`player.leave()` retires the identity, device assignments, view resources and all
UI documents bound to it. It **does not destroy the entity**. Players survive a
controller-script/entity disappearing unless explicitly left; the world owns them.
Scene reload, authored reset, Stop and save restoration create new world-local
identities. Do not persist player/device tokens. Save stable project seat/profile
keys and `saves.reference(entity)`, then create/rebind in `restore()`.

## Devices and assignment

With an interactive input service, reading `players.devices` or creating a player
opts this world into the device adapter. Headless inventory is empty and physical
assignment rejects; identities do not require devices. Each descriptor is `{id,kind,name,connected}`. Kinds are `keyboard`,
`mouse`, `touch`, `gamepad`. Keyboard/mouse/touch identify logical SDL aggregate
routes, not proof that a physical device is attached. Gamepad tokens distinguish
controller instances even with identical names. Unplugging cancels only affected
players' observations; reconnect creates a new token. Lost assignments stay listed
in `player.devices` until released, so projects can display/discuss reassignment.

`player.assign(deviceId, {controls?,region?}={})` returns true or throws TypeError
for invalid/disconnected/conflicting assignments. It does not steal another player's
controls. `player.unassign(deviceId)` returns whether a binding was removed.
Reassign by releasing the old owner first. Failed assignment preserves old state.

- A gamepad or aggregate mouse is exclusive to one player.
- One keyboard may be partitioned with disjoint `controls:['key:W',...]`; empty
  controls means the whole keyboard. Controls are physical binding tokens, not
  logical action names. OS/SDL2 merges physical keyboards/mice: independent raw
  multi-keyboard/multi-mouse hardware streams are **not implemented**. Keyboard
  rollover/ghosting remains a hardware limit.
- A touch assignment uses normalized top-left `{x,y,width,height}` in **window**
  coordinates. Regions must not overlap; shared boundaries are half-open. A
  contact is captured at down and remains with that player while crossing regions,
  until release/cancel. Touch input IDs are scoped to each PlayerInput; combine
  player ID + contact ID when storing shared recognizer state.
- At most 16 simultaneous SDL gamepads, eight players, ten active touches and
  4,096 raw changes/touch transitions per input frame. Overflow cancels/baselines
  observations instead of inventing edges; arbitrary unbounded history is not promised.

Multiple assignments may feed one player. Matching raw device controls sum and
clamp to [-1,1] (relative mouse deltas sum); a release from one source cannot
cancel another still-held source. Touch/physical-mouse primary buttons combine. Active touch and its release
retain pointer-position priority; an idle mouse cannot move a touch release
to its old position before UI processing.
Assignment, map replacement and focus
reset establish a held-state baseline without artificial pressed edges or motion.
Unplugging one controller does not delete a player/entity or retarget its controls.
The global `input` namespace retains its legacy aggregate/selected-controller
semantics for compatibility. Multiplayer gameplay must read **player.input**;
project-wide menus can deliberately use global input.

## Logical input and focus

`player.setInputMap(entries)` replaces that player's logical map transactionally;
entry fields are the same project authoring schema: `name`, `axis`, `bindings`
with required `control`, `scale`, `deadzone`, optional paired-vector settings.
No engine movement/jump/weapon concepts are added.

`player.input` provides `held`, `pressed`, `released`, `axis`, `vector`, `stick`,
`touches`, `pointer`. Stick sides are `left`/`right`. Coordinates remain normalized
**window** coordinates, not automatically remapped to the camera. Snapshots are
copied; IDs are not OS pointers. Multiple fixed readers see the same snapshot;
zero-step frames retain edges and catch-up steps do not repeat them. See M35/M69
input semantics. This first scoped surface does not expose the global stick-history
cursor API; advanced per-player flick-history recognition is not claimed.

Bind a normal document with `document.player = player`; null restores global scope.
The document lays out/clips within `player.viewport`. `onUI` includes `playerId`
(decimal string, null for global UI), retaining the ordinary document/element/type/value.
Per-document focus, pointer capture and sliders remain independent. Global UI
continues to use global input/its primary pointer and selected controller; bind
documents to players for independent navigation. OS mouse capture remains global:
projects release `input.pointerCapture` for mouse-driven menus. Local modal
menus block their player's gameplay input while the shared world continues. UI
callbacks can read owner inputs to resume/reassign. A global modal retains the
existing whole-world pause policy. Menu activation consumes overlapping physical
bindings so confirm does not leak into a gameplay action on resume. Device/reset
cancellation clears stale UI presses. `leave()` unloads bound UI handles safely.

## Viewports and cameras

`player.viewport = {x,y,width,height}` is normalized, top-left window space,
positive and within [0,1]. It scopes HUDs even in shared-screen mode.
`player.setView(pose,fov=70,{near=.1,far=500}={})` enables an independent perspective
view. Pose is an ordinary local-world transform, quaternion orientation; FOV is
vertical degrees, 1..175. Invalid values/ranges throw without partial publication.
`player.clearView()` returns to no dedicated view. With no player views, normal
`world.setView`/authored rendering remains the single shared-screen path.

With dedicated views, render targets reuse existing renderer infrastructure and
are composited in ascending player-ID order. Overlapping viewports intentionally
allow later views to cover earlier ones. Uncovered areas remain background.
Each target is rounded to at least one pixel and capped proportionally at 2,048
pixels on its longest side. The scene is sampled, not simulated, per view. Shadow
maps are shared. Display-target tone mapping and UV orientation are explicit;
existing camera/portal textures retain their old semantics. `player.viewError`
reports allocation errors. Camera policy belongs in presentationUpdate, using
`entity.presentedTransform` for smooth following.

`player.project(worldPoint)` returns the existing `ViewportPoint` shape with x/y
mapped to normalized **window** coordinates. `inside` concerns this view's frustum,
not visibility/occlusion. `player.ray({x:.5,y:.5})` returns `{origin,direction}`
from a normalized **viewport-local** point. No active view/valid dimensions returns
null. Both work without rendering. Use ordinary physics queries for hits.

One audio device/listener remains shared; `world.setView`/existing listener policy
controls it. Players do not get private audio mixes. Weather uses bounded secondary
views and separate history IDs. Planetary LOD still uses the primary world view;
widely separated independent planetary cameras are not a new certified capability.
Views do not provide independent floating origins or separate physics worlds.

## Minimal composition

```js
import {players, world} from 'judas';
const p = players.create('Pilot'); // same project logical map
p.assign('keyboard', {controls:['key:W','key:A','key:S','key:D','key:Space']});
p.entity = world.entity('40'); // any ordinary CharacterMotor entity
// During fixedUpdate:
p.entity.character.accelerate({x:p.input.axis('move_x')*12, y:0, z:-p.input.axis('move_y')*12});
// Flat arena example. Gravity/support remain owned by the motor.
```

See the ordinary [Core Scramble project](../../projects/core_scramble/README.md)
(repository path `projects/core_scramble`) for complete physics gameplay, four
keyboard profiles, gamepad hotplug/reassignment, captured touch controls, local
menus, shared/split camera policy and world lifecycle handling. No C++ party rules.

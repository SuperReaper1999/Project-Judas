# Local players, devices and views

## Ownership and integration

- Optional `RuntimeWorld::LocalPlayers` stores eight identities at most. Process-
  monotonic tokens cannot alias a later player. Identity does not own devices,
  camera policy, entity physics or gameplay. Changing entity/device preserves it.
- `Window` adapts physical events into a bounded, world-owned `InputDeviceHub`.
  Additional gamepads are opened only while the weak hub reference is live.
  Existing global input/selected-controller behavior remains compatible.
- Each player reuses `InputSystem` and ordinary logical maps. Routing occurs once
  per frame before UI; the existing fixed clock latches edges exactly once per
  step. Focus loss, reassignment, overflow and disconnection cancel/baseline input.
  Unrelated player edges survive another controller disconnecting.
- Touch-down captures ownership until release; disjoint normalized regions share
  one device. Sub-frame taps survive routing. Per-player public contact IDs need
  a player-ID prefix in a shared recognizer. SDL primary-touch mouse emulation
  does not duplicate physical-mouse control for another player.
- Ordinary UI documents bind to a player and viewport. Focus, pressed controls and
  sliders are document-local; events include `playerId`. Local modal input blocks
  its owner, global modal pauses the world. Leave unloads bound UI; it does not
  destroy the controlled entity. Entity destruction clears the association getter.
- Dedicated player views sample the same world/presentation alpha into existing
  display render targets; Renderer alone performs GL. Targets tone-map explicitly,
  flip FBO UVs on composition and scale proportionally to a 2048-pixel longest side.
  Failed allocations are bounded/retried on meaningful changes. UI draws once after
  composition. With no dedicated views, the existing single shared-view path runs.
- export export discovers normal project assets/modules; no project-name branches.
  Prefabs/motors/rigid bodies remain ordinary entities. Saves persist project seat
  state plus save-slot entity references, reconstructing transient identities/device/UI
  resources in `restore`. There is no second physics world or networking system.

## Public JudasJS surface

[`players`, `LocalPlayer`, `PlayerInput`](judasjs/local-multiplayer.md) provide
create/get/list/device inventory, safe lifetime, optional entity association,
transactional assignment, maps, observations, viewports/views and projection/rays.
`UIDocument.player` scopes a HUD; `UIEvent.playerId` identifies its owner.
[Types](judas.d.ts), [inventory](judasjs/API_INVENTORY.md), live enumeration and
copyable [cookbook](judasjs/examples/local-multiplayer.js) cover the actual runtime.
Input maps remain game-authored; there are no engine move/jump/weapon methods.

## Core Scramble

[Ordinary project and controls](../projects/core_scramble/README.md): two to four
bumper bots push five physical energy cores into four colored goals during a
90-second round. Finite forces/impulses drive bots/shoves/jumps. Cargo and bots
collide normally. Goals retire a scored body and spawn a distinct replacement.
Fallen bots lose one point and rebind the same identity to a new body.

Shared and split cameras use the same players/score/round. Four keyboard partitions,
controller hotplug, explicit reassignment, touch regions, owner menus, global
lobby, reset and end-of-round state are project JS. An available gamepad is an
**arrival event**, not permission to steal back an explicitly released seat.
Scripts/assets/scenes/UI are editable; original PCM sounds and licensed font assets
are included. No hardware-controller availability is assumed for development.

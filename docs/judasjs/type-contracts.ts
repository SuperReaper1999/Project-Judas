/** Developer-only compile checks, never imported by the Judas runtime.
 * Positive calls must remain accepted; @ts-expect-error calls must remain errors.
 * Actual execution/value validation is covered by native tests and the cookbook.
 */
import {world, physics, Entity, LocalPlayer} from 'judas';

declare const owner: Entity;
declare const player: LocalPlayer;
const origin = {x: 0, y: 0, z: 0};
const direction = {x: 0, y: 0, z: -1};

// Main/local-player camera ranges independently default omitted near/far members.
world.setView({position: origin});
world.setView({position: origin}, 70, {});
world.setView({position: origin}, 70, {far: 2000});
world.setView({position: origin}, 70, {near: 0.2});
player.setView({position: origin}, 70, {far: 2000});

// Secondary camera projection deliberately requires a complete clipping range.
owner.setCameraProjection({near: 0.2, far: 2000});
// @ts-expect-error Missing near is invalid for secondary camera projection.
owner.setCameraProjection({far: 2000});
// @ts-expect-error Unknown range keys must not silently become public API.
world.setView({position: origin}, 70, {distance: 2000});

physics.raycast(origin, direction, 20, {ignored: [owner]});
// @ts-expect-error Nearest cast requires its maximum travel distance.
physics.raycast(origin, direction);
// @ts-expect-error Query filters use safe Entity wrappers, not raw IDs.
physics.raycast(origin, direction, 20, {ignored: [owner.id]});
// @ts-expect-error No implicit shooter-specific convenience API exists.
world.shootGun();

if (owner.character) {
  owner.character.velocity = origin;
  // @ts-expect-error Ordinary motor vectors require all three object components.
  owner.character.velocity = {x: 1, y: 2};
}

// A render-only entity has no rigid physical material snapshot.
const material = owner.physicalMaterial;
if (material) {
  const friction: number = material.friction;
  void friction;
}
// @ts-expect-error Missing-body material reads must be checked for null.
const uncheckedFriction: number = owner.physicalMaterial.friction;
void uncheckedFriction;

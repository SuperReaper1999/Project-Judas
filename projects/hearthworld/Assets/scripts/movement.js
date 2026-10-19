import { input } from 'judas';
import { add, sub, mul, dot, length, norm, tangent, qm, axis, rotate } from './math.js';
export function move(player, dt) {
    const motor = player.entity.character, state = motor.state, u = motor.up, t = player.entity.transform;
    const heading = qm(t.rotation, axis({ x: 0, y: 1, z: 0 }, player.state.yaw));
    const f = norm(tangent(rotate(heading, { x: 0, y: 0, z: -1 }), u)), r = norm(tangent(rotate(heading, { x: 1, y: 0, z: 0 }), u));
    let dir = add(mul(f, input.axis('move_y')), mul(r, input.axis('move_x')));
    if (length(dir) > 1)
        dir = norm(dir);
    const pace = player.grab.held ? 3 : input.held('sprint') ? 6.5 : 4.3, desired = mul(dir, pace), carry = state.supported ? state.supportVelocity : { x: 0, y: 0, z: 0 };
    let velocity = motor.velocity;
    const own = tangent(sub(velocity, carry), u), change = sub(desired, own), next = add(own, mul(norm(change), Math.min(length(change), (state.supported ? 27 : 8) * dt)));
    velocity = add(add(carry, next), mul(u, dot(sub(velocity, carry), u)));
    if (input.pressed('jump') && state.supported)
        velocity = add(velocity, mul(u, 5.4));
    motor.velocity = velocity;
}

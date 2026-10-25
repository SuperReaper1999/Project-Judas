import { input, world } from 'judas';
import { CONFIG } from './definitions.js';
import {
    add,
    sub,
    mul,
    dot,
    length,
    norm,
    tangent,
    qm,
    axis,
    rotate,
} from './math.js';
import { fall, tick } from './survival.js';
export function move(p, dt) {
    const m = p.entity.character,
        s = m.state,
        u = m.up,
        t = p.entity.transform;
    const heading = qm(t.rotation, axis({ x: 0, y: 1, z: 0 }, p.state.yaw));
    const f = norm(tangent(rotate(heading, { x: 0, y: 0, z: -1 }), u)),
        r = norm(tangent(rotate(heading, { x: 1, y: 0, z: 0 }), u));
    let d = add(mul(f, input.axis('move_y')), mul(r, input.axis('move_x')));
    if (length(d) > 1) d = norm(d);
    const water = world.fluidSample(
        add(t.position, { x: 0, y: 0.8, z: 0 }),
        u,
        0.55,
        0.3,
        r,
    );
    const swimming = water.immersion > 0.25,
        sprinting =
            input.held('sprint') &&
            length(d) > 0.1 &&
            p.state.vitals.stamina > 1 &&
            !swimming;
    const pace =
        (sprinting ? CONFIG.sprintSpeed : CONFIG.walkSpeed) /
        (1 + p.state.vitals.injury * 0.12);
    const carry = s.supported ? s.supportVelocity : { x: 0, y: 0, z: 0 },
        own = tangent(sub(m.velocity, carry), u),
        change = sub(mul(d, pace), own);
    let velocity = add(
        add(
            carry,
            add(
                own,
                mul(
                    norm(change),
                    Math.min(length(change), (s.supported ? 25 : 7) * dt),
                ),
            ),
        ),
        mul(u, dot(sub(m.velocity, carry), u)),
    );
    if (input.pressed('jump') && s.supported && p.state.vitals.stamina > 10) {
        velocity = add(velocity, mul(u, CONFIG.jumpSpeed));
        p.state.vitals.stamina -= 8;
    }
    if (swimming) {
        const relax = Math.exp(-2 * water.immersion * dt);
        velocity = add(
            water.velocity,
            mul(sub(velocity, water.velocity), relax),
        );
        m.accelerate(
            add(
                mul(
                    sub(m.gravity, water.acceleration),
                    (-water.density / 950) * water.immersion,
                ),
                mul(
                    add(
                        d,
                        mul(
                            u,
                            input.held('jump')
                                ? 1
                                : input.held('dive')
                                  ? -1
                                  : 0,
                        ),
                    ),
                    7,
                ),
            ),
        );
    }
    if (s.supported && !p.wasGrounded) {
        if (fall(p.state.vitals, p.fallSpeed || 0))
            p.say('Hard landing. A field dressing treats the injury.');
        p.fallSpeed = 0;
    }
    if (!s.supported)
        p.fallSpeed = Math.max(p.fallSpeed || 0, -dot(m.velocity, u));
    p.wasGrounded = s.supported;
    m.velocity = velocity;
    p.immersion = water.immersion;
    const head = world.fluidSample(
        add(t.position, { x: 0, y: 1.6, z: 0 }),
        u,
        0.1,
        0.1,
        r,
    );
    tick(p.state.vitals, dt, {
        sprinting,
        swimming,
        underwater: head.immersion > 0.5,
    });
}

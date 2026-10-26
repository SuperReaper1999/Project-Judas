import { world, physics } from 'judas';
import { add, mul, sub, length, norm, qm, axis, rotate } from './math.js';
export default class Camera {
    constructor(player) {
        this.player = player;
    }
    pose(presented = false) {
        const t = presented
                ? this.player.entity.presentedTransform
                : this.player.entity.transform,
            s = this.player.state;
        const rotation = qm(
            qm(t.rotation, axis({ x: 0, y: 1, z: 0 }, s.yaw)),
            axis(
                { x: 1, y: 0, z: 0 },
                s.pitch + (this.player.combat?.recoil || 0),
            ),
        );
        const eye = add(t.position, rotate(t.rotation, { x: 0, y: 1.6, z: 0 })),
            direction = rotate(rotation, { x: 0, y: 0, z: -1 });
        let position = eye;
        if (s.third) {
            const boom = rotate(rotation, { x: 0.7, y: 0.5, z: 3.5 }),
                d = length(boom),
                n = norm(boom),
                hit = physics.sphereCast(eye, 0.2, n, d, {
                    ignored: [this.player.entity],
                });
            position = add(
                eye,
                mul(n, hit ? Math.max(0, hit.distance - 0.1) : d),
            );
        }
        return { position, rotation, direction, eye };
    }
    hit(distance = 4.5) {
        const p = this.pose(false),
            ignored = [this.player.entity];
        if (this.player.grab?.held?.valid) ignored.push(this.player.grab.held);
        const filter = { ignored };
        if (!this.player.state.third)
            return physics.raycast(p.eye, p.direction, distance, filter);
        // The offset camera chooses the intention; the player's eye must still have
        // an unobstructed physical ray to it. A nearby wall cannot be bypassed.
        const aimDistance = Math.max(30, distance),
            sight = physics.raycast(
                p.position,
                p.direction,
                aimDistance,
                filter,
            ),
            target = sight
                ? sight.point
                : add(p.position, mul(p.direction, aimDistance));
        return physics.raycast(
            p.eye,
            norm(sub(target, p.eye)),
            distance,
            filter,
        );
    }
    publish() {
        const p = this.pose(true);
        world.setView(p, this.player.combat?.aiming ? 52 : 74, {
            near: 0.055,
            far: 650,
        });
        return p;
    }
}

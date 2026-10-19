import { physics, input, saves } from 'judas';
import { add, sub, mul, dot, cross, length, norm, bounded, qm, axis, rotate, inverse } from './math.js';
const matrix = (m, v) => add(add(mul(m.x, v.x), mul(m.y, v.y)), mul(m.z, v.z));
function attitude(target, current) { let q = qm(target, inverse(current)); if (q.w < 0)
    q = { w: -q.w, x: -q.x, y: -q.y, z: -q.z }; const v = { x: q.x, y: q.y, z: q.z }, n = length(v); return n > 1e-6 ? mul(v, 2 * Math.atan2(n, Math.max(0, q.w)) / n) : { x: 0, y: 0, z: 0 }; }
export default class GravityGun {
    constructor(p) { this.p = p; this.held = null; this.turn = 0; this.distance = 2.2; }
    drop() { if (this.p.entity.valid)
        this.p.entity.character.ignore([]); this.held = null; this.p.state.heldRef = null; }
    acquire(hit) {
        const e = hit?.entity;
        if (!e?.valid || e.motionType !== 'dynamic' || !e.hasTag('carryable') || e.mass > 120 || physics.joint(e)) {
            this.p.say('Grab a loose log, crate, barrel or unanchored piece within 4 m.');
            return;
        }
        this.held = e;
        this.p.state.heldRef = saves.reference(e);
        this.p.entity.character.ignore([e]);
        this.direction = this.p.camera.pose().direction;
        this.startHeading = qm(this.p.entity.transform.rotation, axis({ x: 0, y: 1, z: 0 }, this.p.state.yaw));
        this.startRotation = e.transform.rotation;
        this.turn = 0;
        this.p.state.stats.grabs++;
        this.p.feedback('Holding: RMB release / LMB launch / R turn', 'grab', hit.point);
    }
    launch() { const e = this.held; if (!e?.valid)
        return; const direction = this.p.camera.pose().direction; e.applyImpulseAtPoint(mul(direction, e.mass * 8.5), e.transform.position); this.p.state.stats.launched++; this.p.feedback('Launched', 'launch', e.transform.position); this.drop(); }
    tick(dt) {
        if (this.p.state.equipped !== 'gravity') {
            if (this.held)
                this.drop();
            return;
        }
        if (!this.p.blocked && input.pressed('grab')) {
            if (this.held) {
                this.drop();
                this.p.say('Released.');
            }
            else
                this.acquire(this.p.camera.hit(4));
        }
        const e = this.held;
        if (!e)
            return;
        if (!e.valid || e.motionType !== 'dynamic') {
            this.drop();
            return;
        }
        const view = this.p.camera.pose(), u = this.p.entity.character.up, a = Math.acos(Math.max(-1, Math.min(1, dot(this.direction, view.direction))));
        let turnAxis = norm(cross(this.direction, view.direction));
        if (length(turnAxis) < .001)
            turnAxis = u;
        if (a > .001)
            this.direction = norm(rotate(axis(turnAxis, Math.min(a, 3.6 * dt)), this.direction));
        const target = add(add(view.eye, mul(this.direction, this.distance)), mul(u, -.3));
        if (length(sub(e.transform.position, target)) > 4.5) {
            this.drop();
            this.p.say('Grip broke: the object was blocked or too far away.');
            return;
        }
        if (input.pressed('build_rotate'))
            this.turn += Math.PI / 2;
        const acceleration = bounded(sub(add(mul(sub(target, e.transform.position), 70), mul(sub(this.p.entity.character.velocity, e.velocity), 17)), e.gravity.acceleration), 45);
        e.applyForce(mul(acceleration, e.mass));
        const heading = qm(this.p.entity.transform.rotation, axis({ x: 0, y: 1, z: 0 }, this.p.state.yaw)), targetRotation = qm(axis(u, this.turn), qm(qm(heading, inverse(this.startHeading)), this.startRotation));
        e.applyTorque(matrix(e.inertiaWorld, bounded(sub(mul(attitude(targetRotation, e.transform.rotation), 40), mul(e.angularVelocity, 12)), 60)));
    }
}

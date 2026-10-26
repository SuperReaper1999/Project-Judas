// Optional game-side driving, flotation and aerodynamic policy. Judas owns the
// rigid bodies, wheel contacts, shaft inertia, collision and integration.
import { world, input, physics, saves } from 'judas';
import { VEHICLES } from './vehicle-data.js';
import { vehicles } from './vehicle-session.js';
import { getPlayer } from './people-session.js';
import * as Bag from './inventory.js';
import { burn } from './vehicle-rules.js';
import {
    add,
    sub,
    mul,
    dot,
    cross,
    norm,
    length,
    rotate,
    qm,
    axis,
    bounded,
} from './math.js';
const up0 = { x: 0, y: 1, z: 0 };
export const properties = { key: { type: 'string', default: 'car' } };
export default class Vehicle {
    constructor({ entity, properties }) {
        this.entity = entity;
        this.key = properties.key;
        this.def = VEHICLES[this.key];
        if (!this.def) throw new Error('Unknown vehicle ' + this.key);
        let bag = Bag.createBag(this.def.slots, this.def.capacity);
        for (const [id, n] of Object.entries(this.def.stock || {})) {
            const r = Bag.add(bag, id, n);
            if (!r.ok) throw new Error(r.reason);
            bag = r.bag;
        }
        this.state = {
            bag,
            fuel: this.def.fuel,
            health: 100,
            burned: 0,
            distance: 0,
            rotorAngle: 0,
            damageEvents: 0,
        };
        this.impactCooldown = 0;
    }
    start() {
        this.bind();
    }
    restore() {
        this.bind();
    }
    bind() {
        this.reference = saves.reference(this.entity);
        vehicles.set(this.entity.id, this);
        this.previous = this.entity.transform.position;
        this.previousRotorAngle = this.state.rotorAngle;
        this.rotorStep = 0;
        this.visual = this.entity.children.find((e) =>
            e.hasTag('vehicle_visual'),
        );
        this.wheels = this.entity.children.filter((e) => e.wheel);
        this.rotor = this.entity.children.find((e) =>
            e.hasTag('vehicle_rotor'),
        );
    }
    damage(amount) {
        if (
            this.def.kind === 'depot' ||
            !Number.isFinite(amount) ||
            amount <= 0
        )
            return;
        this.state.health = Math.max(0, this.state.health - amount);
        this.state.damageEvents++;
    }
    onCollisionEnter(event) {
        if (this.impactCooldown > 0 || this.def.kind === 'depot') return;
        const closing = Math.abs(dot(event.relativeVelocity, event.normal));
        if (closing > 4) {
            const damage = (closing - 4) ** 2 * 0.7;
            this.damage(damage);
            const p = getPlayer();
            if (p?.state.transport?.driving === this.reference)
                p.state.vitals.health = Math.max(
                    0,
                    p.state.vitals.health - damage * 0.25,
                );
            this.impactCooldown = 0.4;
        }
    }
    fixedUpdate(dt) {
        const d = this.def,
            e = this.entity,
            s = this.state,
            p = getPlayer();
        if (d.kind === 'depot') return;
        this.impactCooldown = Math.max(0, this.impactCooldown - dt);
        const pos = e.transform.position,
            delta = length(sub(pos, this.previous));
        if (delta < 10) s.distance += delta;
        this.previous = pos;
        // All authoring starts parked. No propulsion or flight assist is active
        // during the visible resource-readiness phase.
        const pilot =
                !!p &&
                p.state.transport?.driving === this.reference &&
                p.hud.page !== 'loading',
            powered = pilot && s.fuel > 0 && s.health > 0;
        const throttle = powered ? input.axis('move_y') : 0,
            steer = pilot ? input.axis('move_x') : 0,
            brake = !pilot || input.held('jump'),
            healthFactor = 0.35 + (0.65 * s.health) / 100;
        let command = 0;
        const v = e.velocity,
            omega = e.angularVelocity,
            q = e.transform.rotation,
            forward = rotate(q, { x: 0, y: 0, z: -1 }),
            up = rotate(q, up0),
            right = rotate(q, { x: 1, y: 0, z: 0 }),
            gravity = physics.gravity(pos),
            g = length(gravity),
            referenceUp = g > 0.01 ? norm(mul(gravity, -1)) : up0,
            power = e.powertrain,
            powerState = power.state;
        const wheels = this.wheels;
        if (d.kind === 'car' || d.kind === 'truck') {
            const crank = powerState.shafts.find((s) => s.name === 'crank');
            command = powered
                ? (Math.abs(throttle) > 0.02
                      ? Math.abs(throttle)
                      : Math.max(0, Math.min(0.2, (950 - crank.rpm) * 0.002))) *
                  healthFactor
                : 0;
            const speed = length(v),
                ratio = speed > 20 ? 1 / 2 : speed > 12 ? 1 / 3 : 1 / 4.5;
            power.set({
                sources: [
                    {
                        name: 'engine',
                        input: command,
                        starter: powered && Math.abs(crank.rpm) < 850 ? 1 : 0,
                    },
                ],
                links: [
                    { name: 'gearbox', ratio: ratio * (throttle < 0 ? -1 : 1) },
                    {
                        name: 'clutch',
                        engagement:
                            powered && (Math.abs(throttle) > 0.02 || speed > 2)
                                ? Math.max(
                                      0,
                                      Math.min(
                                          1,
                                          (Math.abs(crank.rpm) - 650) / 600,
                                      ),
                                  )
                                : 0,
                    },
                ],
                brakes: [
                    { name: 'left', engagement: brake ? 1 : 0 },
                    { name: 'right', engagement: brake ? 1 : 0 },
                ],
            });
            wheels.forEach((w, i) =>
                w.wheel.set({
                    driveTorque: 0,
                    brakeTorque: brake ? 1100 : 0,
                    steeringAngle:
                        i < 2
                            ? -Math.min(
                                  0.4,
                                  Math.atan(
                                      ((d.rear - d.front) * 7) /
                                          (speed * speed + 1),
                                  ),
                              ) * steer
                            : 0,
                }),
            );
        } else if (d.kind === 'motorcycle') {
            command = throttle * healthFactor;
            power.set({
                sources: [{ name: 'engine', input: command }],
                brakes: [{ name: 'rear', engagement: brake ? 1 : 0 }],
            });
            wheels.forEach((w, i) =>
                w.wheel.set({
                    driveTorque: 0,
                    brakeTorque: brake ? 500 : 0,
                    steeringAngle: i === 0 ? -0.38 * steer : 0,
                }),
            );
            if (pilot || wheels.some((w) => w.wheel.state.contact))
                e.applyTorque(
                    bounded(
                        sub(mul(cross(up, referenceUp), 1200), mul(omega, 160)),
                        1400,
                    ),
                );
        } else {
            const rotor = powerState.shafts[0],
                spin = Math.abs(rotor.angularVelocity),
                load =
                    d.kind === 'helicopter'
                        ? 0.06
                        : d.kind === 'plane'
                          ? 0.005
                          : 0.002;
            if (d.kind === 'helicopter')
                command = powered
                    ? Math.max(
                          0,
                          Math.min(
                              1,
                              ((d.flight?.hoverInput || 0.523) * (g / 9.81) +
                                  ((input.held('jump')
                                      ? d.flight?.climbSpeed || 5
                                      : 0) -
                                      (input.held('dive')
                                          ? d.flight?.descentSpeed || 3
                                          : 0) -
                                      dot(v, referenceUp)) *
                                      0.035) /
                                  Math.max(0.5, dot(up, referenceUp)),
                          ),
                      ) * healthFactor
                    : 0;
            else if (d.kind === 'plane') {
                if (pilot)
                    p.state.transport.throttle = Math.max(
                        0,
                        Math.min(
                            1,
                            p.state.transport.throttle +
                                input.axis('move_y') * dt * 0.45,
                        ),
                    );
                command = powered
                    ? p.state.transport.throttle * healthFactor
                    : 0;
            } else command = throttle * healthFactor;
            power.set({
                sources: [{ name: 'engine', input: command }],
                brakes: [
                    {
                        name: 'load',
                        engagement: Math.min(1, (spin * spin * load) / 4000),
                    },
                ],
            });
            this.previousRotorAngle = s.rotorAngle;
            this.rotorStep = rotor.angularVelocity * dt;
            s.rotorAngle = (s.rotorAngle + this.rotorStep) % (Math.PI * 2);
            if (d.kind === 'boat')
                this.float(
                    dt,
                    referenceUp,
                    g,
                    forward,
                    command,
                    steer,
                    spin,
                    Math.abs(powerState.sources[0]?.torque || 0) * spin,
                );
            if (d.kind === 'helicopter') {
                e.applyForce(
                    mul(up, (d.rotorLiftCoefficient || 0.502272) * spin * spin),
                );
                e.applyForce(mul(v, -e.mass * (d.flight?.linearDrag || 0.1)));
                if (pilot) {
                    // Assisted flight requests an attitude, never a velocity assignment.
                    // Tilting against residual motion gives a physical return to hover.
                    const horizontalVelocity = sub(
                            v,
                            mul(referenceUp, dot(v, referenceUp)),
                        ),
                        horizontalForward = norm(
                            sub(
                                forward,
                                mul(referenceUp, dot(forward, referenceUp)),
                            ),
                        ),
                        desiredAcceleration = mul(
                            sub(
                                mul(
                                    horizontalForward,
                                    throttle * (d.flight?.cruiseSpeed || 45),
                                ),
                                horizontalVelocity,
                            ),
                            d.flight?.horizontalResponse || 0.6,
                        ),
                        desired = norm(
                            add(
                                referenceUp,
                                bounded(
                                    mul(
                                        desiredAcceleration,
                                        1 / Math.max(g, 0.1),
                                    ),
                                    Math.tan(d.flight?.tilt || 0.48),
                                ),
                            ),
                        );
                    this.stabilize(
                        up,
                        desired,
                        referenceUp,
                        omega,
                        -steer * 0.65,
                        e.mass * 18,
                        e.mass * 3,
                    );
                }
            }
            if (d.kind === 'plane') {
                const airSpeed = Math.max(0, dot(v, forward)),
                    aoa = Math.atan2(-dot(v, up), Math.max(2, airSpeed)),
                    cl = Math.max(-0.3, Math.min(1.7, 0.65 + 4 * aoa));
                e.applyForce(
                    mul(up, 0.5 * 1.225 * airSpeed * airSpeed * 12 * cl),
                );
                e.applyForce(mul(v, -0.5 * 1.225 * length(v) * 0.8));
                const engineTorque = Math.abs(
                        powerState.sources[0]?.torque || 0,
                    ),
                    watts = engineTorque * spin * 0.7;
                e.applyForce(
                    mul(
                        forward,
                        Math.min(6500, watts / Math.max(10, airSpeed)),
                    ),
                );
                wheels.forEach((w, i) =>
                    w.wheel.set({
                        driveTorque: 0,
                        brakeTorque: !pilot || input.held('sprint') ? 1200 : 0,
                        steeringAngle: i === 0 ? -0.35 * steer : 0,
                    }),
                );
                if (pilot) {
                    const pitch =
                            (input.held('jump') ? 0.2 : 0) -
                            (input.held('dive') ? 0.15 : 0),
                        desired = norm(
                            add(
                                add(referenceUp, mul(forward, -pitch)),
                                mul(right, steer * 0.3),
                            ),
                        );
                    this.stabilize(
                        up,
                        desired,
                        referenceUp,
                        omega,
                        -steer * 0.35 * Math.min(1, airSpeed / 20),
                        e.mass * 10,
                        e.mass * 2,
                    );
                }
            }
        }
        if (powered)
            this.state = burn(
                s,
                dt * d.burn * (0.25 + 0.75 * Math.abs(command)),
            );
    }
    stabilize(up, target, referenceUp, omega, yaw, gain, damping) {
        const desiredOmega = mul(referenceUp, yaw);
        this.entity.applyTorque(
            bounded(
                sub(
                    mul(cross(up, target), gain),
                    mul(sub(omega, desiredOmega), damping),
                ),
                gain,
            ),
        );
    }
    float(dt, up, g, forward, command, steer, spin, watts) {
        const e = this.entity,
            q = e.transform.rotation;
        let wet = 0;
        for (const x of [-0.55, 0.55])
            for (const z of [-1.45, 1.45]) {
                const point = add(
                        e.transform.position,
                        rotate(q, { x, y: -0.15, z }),
                    ),
                    sample = world.fluidSample(point, up, 0.35, 0.1, forward);
                if (sample.immersion <= 0) continue;
                wet++;
                const velocity = sub(e.pointVelocity(point), sample.velocity),
                    lift = mul(
                        up,
                        sample.density * 0.25 * g * sample.immersion,
                    ),
                    drag = mul(
                        velocity,
                        ((-e.mass * 0.7) / 4) * (input.held('jump') ? 4 : 1),
                    );
                e.applyImpulseAtPoint(mul(add(lift, drag), dt), point);
            }
        if (wet) {
            const speed = Math.abs(dot(e.velocity, forward));
            e.applyForce(
                mul(
                    forward,
                    Math.sign(command) *
                        Math.min(
                            spin * spin * 0.08,
                            (watts * 0.65) / Math.max(2, speed),
                        ),
                ),
            );
            e.applyTorque(
                mul(
                    up,
                    -steer *
                        (speed * speed * 20 +
                            Math.abs(command) * spin * spin * 0.006),
                ),
            );
            e.applyTorque(mul(e.angularVelocity, -e.mass * 0.8));
        }
    }
    update(dt) {
        if (this.def.kind === 'depot' || !this.entity.audio?.ready) return;
        this.audioTime = (this.audioTime || 0) - dt;
        if (this.audioTime > 0) return;
        this.audioTime = 0.15;
        const p = getPlayer(),
            running =
                p?.state.transport?.driving === this.reference &&
                this.state.fuel > 0 &&
                this.state.health > 0 &&
                !p.hud.modal;
        if (running) {
            const rpm = Math.abs(
                this.entity.powertrain.state.shafts[0]?.rpm || 0,
            );
            this.entity.setAudio({
                pitch: Math.max(0.6, Math.min(1.8, 0.7 + rpm / 5000)),
                volume: 0.2,
            });
            if (!this.entity.audio.playing) this.entity.playAudio();
        } else if (this.entity.audio.playing) this.entity.stopAudio();
    }
    presentationUpdate(dt, alpha) {
        // Body-free shell children already inherit the parent's interpolated pose.
        // Writing an interpolated world pose here would convert it through the
        // authoritative parent and then interpolate the parent a second time.
        // Only animate the rotor's local rotation, via an authoritative world pose.
        if (!this.rotor?.valid) return;
        const rotor = this.rotor,
            t = this.entity.transform;
        rotor.transform = {
            position: add(
                t.position,
                rotate(
                    t.rotation,
                    this.def.kind === 'helicopter'
                        ? { x: 0, y: 1.02, z: 0 }
                        : { x: 0, y: 0, z: -3 },
                ),
            ),
            rotation: qm(
                t.rotation,
                axis(
                    this.def.kind === 'helicopter' ? up0 : { x: 0, y: 0, z: 1 },
                    (this.previousRotorAngle ?? this.state.rotorAngle) +
                        this.rotorStep * alpha,
                ),
            ),
        };
    }
    destroy() {
        if (vehicles.get(this.entity.id) === this)
            vehicles.delete(this.entity.id);
    }
}

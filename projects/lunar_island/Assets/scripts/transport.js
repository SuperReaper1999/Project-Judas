import { world, input, physics, saves, scenes } from 'judas';
import { vehicleFor, vehicleByReference, vehicles } from './vehicle-session.js';
import * as Bag from './inventory.js';
import * as Rules from './vehicle-rules.js';
import { add, sub, mul, norm, length, rotate, qm, axis } from './math.js';
import { tick } from './survival.js';
export default class Transport {
    constructor(player) {
        this.p = player;
        if (!player.state.transport)
            player.state.transport = { driving: null, throttle: 0 };
        this.pending = null;
        this.interestTime = 0;
    }
    get current() {
        return vehicleByReference(this.p.state.transport.driving);
    }
    get driving() {
        return !!this.p.state.transport.driving;
    }
    near(v) {
        return (
            !!v?.entity.valid &&
            length(
                sub(
                    v.entity.transform.position,
                    this.p.entity.transform.position,
                ),
            ) < 6
        );
    }
    interact(hit) {
        const v = vehicleFor(hit?.entity);
        if (!v) return false;
        this.p.hud.vehicle = v.reference;
        this.p.hud.open('vehicle');
        return true;
    }
    context(hit) {
        const v = vehicleFor(hit?.entity);
        return v ? 'E · ' + v.def.name + ' / cargo & controls' : '';
    }
    openCargo(ref) {
        const v = vehicleByReference(ref);
        if (
            !this.near(v) ||
            (v.entity.motionType === 'dynamic' && length(v.entity.velocity) > 2)
        ) {
            this.p.say('Stop beside the vehicle to open its cargo.');
            return;
        }
        this.p.hud.openStorage(ref, true);
    }
    cargoName(ref) {
        return vehicleByReference(ref)?.def.name || 'Storage';
    }
    cargo(ref) {
        return vehicleByReference(ref)?.state.bag;
    }
    transfer(ref, index, store, whole) {
        const v = vehicleByReference(ref);
        if (!this.near(v)) return;
        const from = store ? this.p.state.bag : v.state.bag,
            to = store ? v.state.bag : this.p.state.bag,
            slot = from.slots[index];
        if (!slot) return;
        const r = Bag.transfer(from, to, slot.item, whole ? slot.count : 1);
        if (r.ok) {
            if (store) {
                this.p.state.bag = r.from;
                v.state.bag = r.to;
            } else {
                v.state.bag = r.from;
                this.p.state.bag = r.to;
            }
            this.p.reconcileEquipment();
            this.p.say('Cargo transferred.');
        } else this.p.say(r.reason);
    }
    service(ref, kind) {
        const v = vehicleByReference(ref);
        if (!this.near(v) || v.def.kind === 'depot') return;
        const r = Rules.service(this.p.state.bag, v.state, v.def, kind);
        if (r.ok) {
            this.p.state.bag = r.bag;
            v.state = r.state;
            this.p.say(r.message);
        } else this.p.say(r.reason);
    }
    requestEnter(ref) {
        const v = vehicleByReference(ref);
        if (!this.near(v) || v.def.kind === 'depot') return;
        this.pending = ref;
        this.p.hud.close();
    }
    fixedUpdate(dt) {
        const p = this.p;
        this.interestTime -= dt;
        if (this.interestTime <= 0) {
            scenes.setInterest('lunar-travel', p.entity.transform.position, {
                load: 120,
                retain: 180,
            });
            this.interestTime = 0.5;
        }
        if (this.pending) {
            const v = vehicleByReference(this.pending);
            this.pending = null;
            if (
                this.near(v) &&
                v.state.health > 0 &&
                length(v.entity.velocity) < 3
            ) {
                if (scenes.owner(v.entity) !== 'root') scenes.adopt(v.entity);
                p.state.transport.driving = v.reference;
                p.state.transport.throttle = 0;
                p.entity.character.enabled = false;
                p.state.third = true;
                p.state.yaw = 0;
                p.state.pitch = -0.2;
                p.say(
                    'WASD drive / steer · Space brake or climb · Ctrl descend · E exit · Tab cargo',
                );
            } else p.say('Stop the vehicle and repair it before entering.');
        }
        if (!this.driving) return false;
        const v = this.current;
        if (!v) {
            p.say('Waiting for the saved vehicle to become available.');
            return true;
        }
        p.entity.character.enabled = false;
        p.entity.transform = {
            position: add(
                v.entity.transform.position,
                rotate(v.entity.transform.rotation, { x: 0, y: 0.3, z: 0 }),
            ),
            rotation: v.entity.transform.rotation,
        };
        const head = add(
            v.entity.transform.position,
            rotate(v.entity.transform.rotation, { x: 0, y: 1, z: 0 }),
        );
        const water = world.fluidSample(head, { x: 0, y: 1, z: 0 }, 0.2, 0.1, {
            x: 1,
            y: 0,
            z: 0,
        });
        p.immersion = water.immersion;
        tick(p.state.vitals, dt, { underwater: water.immersion > 0.5 });
        if (input.pressed('interact')) this.exit();
        return true;
    }
    exit() {
        const p = this.p,
            v = this.current;
        if (!v) return false;
        if (length(v.entity.velocity) > 3) {
            p.say('Brake and stop before leaving.');
            return false;
        }
        const t = v.entity.transform;
        const candidates = [
            { x: v.def.half[0] + 1.1, y: 0.2, z: 0 },
            { x: -v.def.half[0] - 1.1, y: 0.2, z: 0 },
            { x: 0, y: 0.2, z: v.def.half[2] + 1.2 },
        ];
        if (v.def.kind === 'boat') candidates.unshift(candidates.pop());
        // Prefer solid dock/shore exits before falling back to open water.
        for (const requireGround of v.def.kind === 'boat'
            ? [true, false]
            : [true]) {
            for (const offset of candidates) {
                let foot = add(t.position, rotate(t.rotation, offset));
                const ground = physics.raycast(
                    add(foot, { x: 0, y: 2, z: 0 }),
                    { x: 0, y: -1, z: 0 },
                    6,
                    { ignored: [v.entity, p.entity] },
                );
                if (!ground && requireGround) {
                    if (v.def.kind !== 'boat')
                        p.say('Land before leaving the aircraft.');
                    continue;
                }
                if (ground)
                    foot = { x: foot.x, y: ground.point.y + 0.08, z: foot.z };
                const clear = physics.capsuleCast(
                    {
                        position: add(foot, { x: 0, y: 0.9, z: 0 }),
                        rotation: { w: 1, x: 0, y: 0, z: 0 },
                    },
                    0.3,
                    0.6,
                    { x: 0, y: 1, z: 0 },
                    0,
                    { ignored: [p.entity] },
                );
                if (clear) continue;
                p.entity.transform = {
                    position: foot,
                    rotation: { w: 1, x: 0, y: 0, z: 0 },
                };
                p.entity.character.enabled = true;
                p.entity.character.velocity = v.entity.pointVelocity(foot);
                p.state.transport.driving = null;
                p.state.transport.throttle = 0;
                p.state.yaw = 0;
                p.state.pitch = -0.1;
                p.state.third = true;
                p.say('On foot. E opens vehicle controls and cargo.');
                return true;
            }
        }
        p.say('Exit blocked. Move to a clear, level stopping place.');
        return false;
    }
    pose(presented = false) {
        const v = this.current;
        if (!v) return null;
        const t = presented ? v.entity.presentedTransform : v.entity.transform,
            s = this.p.state,
            heading = qm(t.rotation, axis({ x: 0, y: 1, z: 0 }, s.yaw)),
            rotation = qm(heading, axis({ x: 1, y: 0, z: 0 }, s.pitch));
        const pivot = add(
            t.position,
            rotate(t.rotation, { x: 0, y: 0.6, z: 0 }),
        );
        let pos;
        if (s.third) {
            const distance = Math.max(8, v.def.half[2] * 3),
                direction = rotate(rotation, { x: 0, y: 0, z: 1 });
            const hit = physics.sphereCast(pivot, 0.2, direction, distance, {
                ignored: [v.entity, this.p.entity],
            });
            pos = add(
                pivot,
                mul(
                    direction,
                    hit ? Math.max(0.3, hit.distance - 0.2) : distance,
                ),
            );
        } else
            pos = add(
                t.position,
                rotate(t.rotation, {
                    x: 0,
                    y: Math.max(1.2, v.def.half[1] + 0.5),
                    z: -v.def.half[2] * 0.7,
                }),
            );
        return {
            position: pos,
            eye: pos,
            rotation,
            direction: rotate(rotation, { x: 0, y: 0, z: -1 }),
        };
    }
    status() {
        const v = this.current;
        if (!v) return '';
        const speed = length(v.entity.velocity) * 3.6,
            rpm = v.entity.powertrain?.state.shafts[0]?.rpm || 0;
        return `${v.def.name} · ${speed.toFixed(0)} km/h · ${v.state.fuel.toFixed(1)} L · ${v.state.health.toFixed(0)}% · ${Math.abs(rpm).toFixed(0)} RPM\n${v.def.kind === 'plane' ? 'W/S throttle · A/D turn · Space up / Ctrl down · Shift wheel brake' : v.def.kind === 'helicopter' ? 'W/S tilt · A/D yaw · Space climb / Ctrl descend' : 'W/S forward/reverse · A/D steer · Space brake'} · E exit · Tab cargo`;
    }
}

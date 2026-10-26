// Gameplay-owned hitscan, magazines and melee. The engine only supplies queries,
// body impulses, motor movement, audio and particles.
import { world, physics, input } from 'judas';
import { vehicleFor } from './vehicle-session.js';
import { WEAPONS, PEOPLE } from './encounter-data.js';
import * as Bag from './inventory.js';
import * as Rules from './encounter-rules.js';
import { actors, actorFor, setPlayer, clearPlayer } from './people-session.js';
import { add, sub, mul, norm, length, rotate, axis, qm } from './math.js';
export default class Combat {
    constructor(player) {
        this.p = player;
        setPlayer(player);
        if (!player.state.combat)
            player.state.combat = {
                magazines: {},
                reload: null,
                shots: 0,
                kills: 0,
                wolves: 0,
                quests: {},
                gift: false,
                lastTarget: null,
            };
        this.recoil = 0;
        this.lastShot = null;
        this.hitTime = 0;
    }
    get state() {
        return this.p.state.combat;
    }
    get weapon() {
        return WEAPONS[this.p.state.equipped];
    }
    get aiming() {
        return !!this.weapon?.ammo && input.held('aim') && !this.p.hud.modal;
    }
    tick(dt) {
        this.recoil *= Math.exp(-10 * dt);
        this.hitTime = Math.max(0, this.hitTime - dt);
        const reload = this.state.reload;
        if (reload) {
            if (
                this.p.state.equipped !== reload.weapon ||
                !Bag.count(this.p.state.bag, reload.weapon)
            ) {
                this.state.reload = null;
                return;
            }
            reload.remaining -= dt;
            if (reload.remaining <= 0) {
                const def = WEAPONS[reload.weapon],
                    r = Rules.reload(
                        this.p.state.bag,
                        def,
                        this.state.magazines[reload.weapon] || 0,
                    );
                if (r.ok) {
                    this.p.state.bag = r.bag;
                    this.state.magazines[reload.weapon] = r.magazine;
                    this.p.effect('reload', 'Reloaded ' + def.name + '.');
                } else this.p.say(r.reason);
                this.state.reload = null;
            }
        }
        if (input.pressed('reload') && !this.p.builder.active) this.reload();
    }
    reload() {
        const w = this.weapon,
            id = this.p.state.equipped;
        if (!w?.ammo || this.state.reload) return false;
        if (
            (this.state.magazines[id] || 0) >= w.capacity ||
            !Bag.count(this.p.state.bag, w.ammo)
        ) {
            this.p.say('Magazine full or no compatible reserve rounds.');
            return false;
        }
        this.state.reload = { weapon: id, remaining: w.reload };
        this.p.say('Reloading ' + w.name + '…');
        return true;
    }
    fire() {
        const p = this.p,
            w = this.weapon || WEAPONS.hands,
            id = p.state.equipped;
        if (w.ammo) {
            if (this.state.reload) {
                p.say('Finish reloading first.');
                return true;
            }
            if (!Bag.count(p.state.bag, id)) {
                p.reconcileEquipment();
                return true;
            }
            const mag = this.state.magazines[id] || 0;
            if (!Number.isSafeInteger(mag) || mag <= 0 || mag > w.capacity) {
                p.say('Empty magazine · R reloads from your backpack.');
                return true;
            }
            this.state.magazines[id] = mag - 1;
            this.state.shots++;
        } else if (!WEAPONS[id] && id !== 'hands') return false;
        p.cooldown = w.interval;
        p.swing = 0.3;
        const pose = p.camera.pose(),
            sight = p.camera.hit(w.range),
            target = sight
                ? sight.point
                : add(pose.eye, mul(pose.direction, w.range));
        const direction = norm(sub(target, pose.eye));
        let hitAny = false;
        const pattern = w.pellets
            ? [
                  [0, 0],
                  [-1, 0],
                  [1, 0],
                  [0, 1],
                  [0, -1],
                  [0.7, 0.7],
              ]
            : [[0, 0]];
        for (const [x, y] of pattern) {
            const spread = w.pellets ? 0.018 : 0,
                dir = norm(
                    add(
                        direction,
                        rotate(pose.rotation, {
                            x: x * spread,
                            y: y * spread,
                            z: 0,
                        }),
                    ),
                );
            const hit = physics.raycast(pose.eye, dir, w.range, {
                ignored: [p.entity],
            });
            this.lastShot = {
                entity: hit?.entity?.id || null,
                point: hit?.point || null,
            };
            this.state.lastTarget = hit?.entity?.id || null;
            if (!hit?.entity?.valid) continue;
            const actor = actorFor(hit.entity);
            if (actor && !actor.state.dead) {
                if (actor.hurt(w.damage, dir, id)) {
                    this.hitTime = 0.16;
                    hitAny = true;
                }
            } else if (hit.entity.motionType === 'dynamic') {
                vehicleFor(hit.entity)?.damage(w.damage * 0.4);
                hit.entity.applyImpulseAtPoint(
                    mul(dir, w.impulse || 12),
                    hit.point,
                );
            }
            const fx = world.entity(PEOPLE.impact);
            if (fx?.valid) {
                fx.transform = { position: hit.point };
                fx.burst(5);
            }
        }
        if (w.ammo) {
            this.recoil = Math.min(0.22, this.recoil + (w.recoil || 0.04));
            p.effect('gunshot', hitAny ? 'Hit confirmed.' : 'Shot fired.');
            for (const a of actors.values())
                a.hear(p.entity.transform.position);
        } else p.effect('chop', hitAny ? 'Strike connected.' : 'Melee swing.');
        return true;
    }
    interact(hit) {
        const a = actorFor(hit?.entity);
        if (!a) return false;
        if (!a.def.friendly && !a.state.dead) {
            this.p.say(a.def.name + ' is hostile.');
            return true;
        }
        this.p.hud.person = a.key;
        this.p.hud.open(a.state.dead ? 'corpse' : 'dialogue');
        return true;
    }
    context(hit) {
        const a = actorFor(hit?.entity);
        return a
            ? a.state.dead
                ? 'E · Loot ' + a.def.name
                : a.def.friendly
                  ? 'E · Speak with ' + a.def.name
                  : a.def.name + ' / ' + a.state.mode
            : '';
    }
    status() {
        const w = this.weapon,
            id = this.p.state.equipped;
        if (!w?.ammo) return this.hitTime ? 'HIT' : '';
        return `${w.name.toUpperCase()}  ${this.state.magazines[id] || 0}/${w.capacity}  · reserve ${Bag.count(this.p.state.bag, w.ammo)}${this.state.reload ? ' · RELOADING' : ' · R reload'}${this.hitTime ? '  HIT' : ''}`;
    }
    destroy() {
        clearPlayer(this.p);
    }
}

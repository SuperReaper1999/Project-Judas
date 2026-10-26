// NPC policy is project JavaScript. Navigation is advisory; CharacterMotor owns
// collision-resolved locomotion. All durable quantities live in this.state.
import { world, physics, saves, console } from 'judas';
import { PEOPLE } from './encounter-data.js';
import * as Bag from './inventory.js';
import * as Rules from './encounter-rules.js';
import { actors, corpses, getPlayer } from './people-session.js';
import {
    add,
    sub,
    mul,
    length,
    norm,
    dot,
    tangent,
    axis,
    qm,
    vec,
} from './math.js';
export const properties = { key: { type: 'string', default: 'mara' } };
export default class Person {
    constructor({ entity, properties }) {
        this.entity = entity;
        this.key = properties.key;
        this.def = PEOPLE.actors[this.key];
        if (!this.def) throw new Error('Unknown NPC profile ' + this.key);
        let bag = Bag.createBag(18, 200);
        for (const [id, n] of Object.entries(this.def.stock || {})) {
            const r = Bag.add(bag, id, n);
            if (!r.ok) throw new Error('NPC stock: ' + r.reason);
            bag = r.bag;
        }
        this.state = {
            health: this.def.health,
            dead: false,
            corpse: null,
            bag,
            mode: 'patrol',
            waypoint: 0,
            timer: 0,
            attack: 0,
            windup: 0,
            lastSeen: null,
            alert: 0,
            dialogue: 0,
        };
        this.repath = 0;
        this.clip = '';
        this.lastMove = null;
        this.stuck = 0;
    }
    start() {
        this.bind();
    }
    restore() {
        this.bind();
    }
    bind() {
        actors.set(this.entity.id, this);
        this.visual = this.entity.children[0];
        this.held = this.entity.children[1];
        if (this.state.corpse) {
            const e = saves.resolve(this.state.corpse);
            if (e?.valid) corpses.set(e.id, this);
        }
        if (this.state.dead) {
            this.entity.character.enabled = false;
            if (this.entity.navigation.enabled) this.entity.navigation.clear();
            this.entity.navigation.enabled = false;
            if (this.visual?.valid) this.visual.renderVisible = false;
        }
    }
    hear(position) {
        if (
            this.def.friendly ||
            this.state.dead ||
            length(sub(position, this.entity.transform.position)) > 45
        )
            return;
        this.state.lastSeen = position;
        this.state.alert = 8;
    }
    hurt(amount, direction, weapon) {
        if (this.def.friendly) {
            getPlayer()?.say('Survivors are protected from friendly fire.');
            return false;
        }
        if (this.state.dead || !Number.isFinite(amount) || amount <= 0)
            return false;
        this.state.health = Math.max(0, this.state.health - amount);
        this.hear(
            getPlayer()?.entity.transform.position ||
                this.entity.transform.position,
        );
        this.state.attack = Math.max(0.4, this.state.attack);
        if (this.state.health === 0) this.die(direction);
        return true;
    }
    die(direction = { x: 0, y: 0, z: 1 }) {
        if (this.state.dead) return;
        let corpse;
        try {
            const position = add(this.entity.transform.position, {
                x: 0,
                y: this.def.animal ? 0.45 : 0.9,
                z: 0,
            });
            corpse = world.spawnPrefab(
                this.def.animal ? PEOPLE.wolfCorpse : PEOPLE.humanCorpse,
                { position, rotation: axis({ x: 0, y: 0, z: 1 }, 0.5) },
            );
            corpse.applyImpulseAtPoint(mul(direction, 18), position);
            this.state.corpse = saves.reference(corpse);
        } catch (error) {
            if (corpse?.valid) corpse.destroy();
            this.state.mode = 'down';
            this.state.timer = 0.5;
            console.log('Corpse creation deferred: ' + error.message);
            return;
        }
        this.state.dead = true;
        this.state.mode = 'dead';
        this.entity.character.enabled = false;
        if (this.entity.navigation.enabled) this.entity.navigation.clear();
        this.entity.navigation.enabled = false;
        this.visual.renderVisible = false;
        corpses.set(corpse.id, this);
        const p = getPlayer();
        if (p) {
            p.state.combat[this.def.animal ? 'wolves' : 'kills']++;
            p.say(
                this.def.name +
                    ' defeated. E on the body to recover finite loot.',
            );
        }
    }
    canSee(p) {
        const here = add(this.entity.transform.position, {
                x: 0,
                y: this.def.animal ? 0.6 : 1.4,
                z: 0,
            }),
            there = add(p.entity.transform.position, { x: 0, y: 1, z: 0 }),
            delta = sub(there, here),
            distance = length(delta);
        return (
            distance < this.def.sight &&
            !physics.raycast(here, norm(delta), distance, {
                ignored: [this.entity, p.entity],
            })
        );
    }
    fixedUpdate(dt) {
        const p = getPlayer(),
            m = this.entity.character,
            s = this.state;
        if (!p || !p.hud || s.dead) return;
        if (p.hud.page === 'loading') {
            m.velocity = { x: 0, y: 0, z: 0 };
            return;
        }
        if (s.health <= 0) {
            s.timer -= dt;
            if (s.timer <= 0) this.die();
            return;
        }
        s.timer -= dt;
        s.attack = Math.max(0, s.attack - dt);
        s.alert = Math.max(0, s.alert - dt);
        this.repath -= dt;
        const position = this.entity.transform.position,
            delta = sub(p.entity.transform.position, position),
            distance = length(delta),
            visible = this.canSee(p) && p.state.vitals.health > 0;
        if (!this.def.friendly && visible) {
            s.lastSeen = p.entity.transform.position;
            s.alert = 5;
        }
        let target = vec(this.def.route[s.waypoint % this.def.route.length]),
            speed = this.def.speed;
        const ranged = !!this.def.ranged && Bag.count(s.bag, 'ammo_rifle') > 0;
        s.mode = 'patrol';
        if (!this.def.friendly && s.alert > 0) {
            target = s.lastSeen || target;
            s.mode = 'pursuit';
            if (s.health < this.def.health * 0.25 && !this.def.animal) {
                target = add(
                    position,
                    mul(norm(sub(position, p.entity.transform.position)), 7),
                );
                s.mode = 'retreat';
                speed *= 1.15;
            }
        } else if (length(sub(position, target)) < 1.3) {
            if (s.timer <= 0) {
                s.waypoint++;
                s.timer = 3;
            }
            speed = 0;
        }
        if (this.def.friendly && distance < 4.5) {
            speed = 0;
            s.mode = 'conversation';
            this.facing = Math.atan2(delta.x, delta.z);
        }
        const inRange = distance < (ranged ? 13 : 1.9);
        if (!this.def.friendly && visible && inRange && s.mode !== 'retreat') {
            speed = 0;
            s.mode = 'attack';
            if (s.attack <= 0 && !s.windup) {
                s.windup = 0.45;
                s.attack = ranged ? 1.7 : 1.25;
                s.firing = ranged;
                this.play('Attack');
            }
        }
        if (s.windup > 0) {
            s.windup -= dt;
            if (s.windup <= 0) {
                s.windup = 0;
                if (s.firing) {
                    const spent = Bag.remove(s.bag, 'ammo_rifle', 1);
                    if (!spent.ok) {
                        s.firing = false;
                        return;
                    }
                    s.bag = spent.bag;
                    const sound = world.entity(PEOPLE.sounds.gunshot);
                    try {
                        sound.transform = {
                            position: add(position, { x: 0, y: 1.3, z: 0 }),
                        };
                        sound.playAudioOneShot();
                    } catch (error) {
                        if (!this.audioWarning) {
                            console.log(
                                'Optional NPC shot audio skipped: ' +
                                    error.message,
                            );
                            this.audioWarning = true;
                        }
                    }
                }
                if (
                    !this.def.friendly &&
                    this.canSee(p) &&
                    length(sub(p.entity.transform.position, position)) <
                        (s.firing ? 15 : 2.2)
                ) {
                    const armour =
                        p.state.equipment.body === 'work_coat' ? 0.9 : 1;
                    p.state.vitals.health = Math.max(
                        0,
                        p.state.vitals.health - this.def.damage * armour,
                    );
                    p.say(
                        this.def.name +
                            (s.firing
                                ? ' fired from the camp.'
                                : ' struck you.'),
                    );
                    p.effect('hurt', p.notice);
                }
            }
        }
        if (this.repath <= 0 && speed > 0) {
            this.repath = 0.6;
            this.entity.navigation.setDestination(target);
        }
        const state = this.entity.navigation.state;
        let desired = speed > 0 ? state.steering : { x: 0, y: 0, z: 0 };
        if (length(desired) > 0)
            desired = mul(tangent(desired, m.up), speed / this.def.speed);
        m.velocity = add(desired, mul(m.up, dot(m.velocity, m.up)));
        // Runtime barricades intentionally remain physical obstacles. If a route
        // is blocked by one, attack only a barrier actually within reach.
        if (
            !this.def.friendly &&
            s.alert > 0 &&
            s.attack === 0 &&
            distance > 2
        ) {
            const origin = add(position, {
                    x: 0,
                    y: this.def.animal ? 0.6 : 1.4,
                    z: 0,
                }),
                h = physics.raycast(origin, norm(delta), 1.8, {
                    ignored: [this.entity],
                });
            if (
                h &&
                p.housing.damageFromActor(h, this.entity, this.def.damage)
            ) {
                s.attack = 1.25;
                this.play('Attack');
            }
        }
        this.walking = length(desired) > 0.1;
        if (this.walking) {
            this.facing = this.def.animal
                ? Math.atan2(-desired.z, desired.x)
                : Math.atan2(desired.x, desired.z);
        }
        if (!s.windup)
            this.play(this.walking ? (speed > 2.8 ? 'Run' : 'Walk') : 'Idle');
    }
    play(name) {
        if (
            !this.visual?.valid ||
            !this.visual.animation?.info?.ready ||
            this.clip === name ||
            this.visual.animation.info.transitioning
        )
            return;
        this.visual.animation.crossFade(name, 0.12);
        this.visual.animation.loop = name !== 'Attack';
        this.clip = name;
    }
    presentationUpdate() {
        if (this.state.dead && this.held?.valid)
            this.held.renderVisible = false;
        if (!this.visual?.valid || this.state.dead) return;
        const pose = this.entity.presentedTransform;
        this.visual.transform = {
            position: pose.position,
            rotation: qm(
                pose.rotation,
                axis({ x: 0, y: 1, z: 0 }, this.facing || 0),
            ),
            scale: { x: 1, y: 1, z: 1 },
        };
        this.presentHeld();
    }
    presentHeld() {
        if (!this.held?.valid) return;
        const hand = this.visual.animation.jointTransform(
            'hand.R',
            'world',
            true,
        );
        this.held.renderVisible = !!hand && !this.state.dead;
        if (hand)
            this.held.transform = {
                position: hand.position,
                rotation: qm(
                    hand.rotation,
                    axis({ x: 1, y: 0, z: 0 }, Math.PI / 2),
                ),
                scale: { x: 0.45, y: 0.45, z: 0.45 },
            };
    }
    get near() {
        const p = getPlayer();
        return (
            p &&
            length(
                sub(
                    (this.state.dead
                        ? saves.resolve(this.state.corpse)
                        : this.entity
                    )?.transform.position || this.entity.transform.position,
                    p.entity.transform.position,
                ),
            ) < 4.5
        );
    }
    offer(p) {
        if (!this.near) return 'Move closer to speak.';
        const q = PEOPLE.quests[this.def.quest];
        if (!q)
            return this.def.lines?.[0] || 'There is no work available here.';
        const r = Rules.quest(
            p.state.bag,
            p.state.combat.quests[q.id],
            p.state.combat,
            q,
        );
        if (r.ok) {
            p.state.bag = r.bag;
            p.state.combat.quests[q.id] = r.stage;
            return r.message;
        }
        return r.reason;
    }
    trade(p, item, buy, amount = 1) {
        if (!this.near || !this.def.friendly) return;
        const price = this.def.prices?.[item];
        const r = Rules.trade(
            p.state.bag,
            this.state.bag,
            item,
            buy ? price : Math.max(1, Math.floor(price * 0.5)),
            buy,
            amount,
        );
        if (r.ok) {
            p.state.bag = r.player;
            this.state.bag = r.vendor;
            p.reconcileEquipment();
            p.say(
                (buy ? 'Bought ' : 'Sold ') +
                    amount +
                    ' ' +
                    item +
                    '. Stock and tokens transferred.',
            );
        } else p.say(r.reason);
    }
    loot(p, index) {
        if (!this.near || !this.state.dead) return;
        const slot = this.state.bag.slots[index];
        if (!slot) return;
        const r = Bag.transfer(
            this.state.bag,
            p.state.bag,
            slot.item,
            slot.count,
        );
        if (r.ok) {
            this.state.bag = r.from;
            p.state.bag = r.to;
            p.say('Recovered ' + slot.count + ' ' + slot.item + '.');
        } else p.say(r.reason);
    }
    destroy() {
        actors.delete(this.entity.id);
        const corpse = this.state.corpse && saves.resolve(this.state.corpse);
        if (corpse?.valid) corpses.delete(corpse.id);
    }
}

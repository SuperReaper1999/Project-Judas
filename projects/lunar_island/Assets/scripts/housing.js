// Project-owned world interaction. Native entities, joints and save references
// own physical identity; this ledger owns items, ownership and repair policy.
import { world, physics, saves } from 'judas';
import { HOUSING } from './housing-data.js';
import * as Bag from './inventory.js';
import { add, sub, mul, length, vec, qm, inverse, rotate } from './math.js';
const identity = { w: 1, x: 0, y: 0, z: 0 };
const hingeFrame = { w: Math.SQRT1_2, x: 0, y: 0, z: Math.SQRT1_2 };
export function connect(entity, support, point) {
    const a = entity.transform,
        b = support.transform;
    const j = physics.createJoint(entity, {
        type: 'fixed',
        bodyA: entity,
        bodyB: support,
        anchorA: rotate(inverse(a.rotation), sub(point, a.position)),
        anchorB: rotate(inverse(b.rotation), sub(point, b.position)),
        frameA: identity,
        frameB: qm(inverse(b.rotation), a.rotation),
        enabled: true,
    });
    if (!j?.valid) throw new Error('Physical connection unavailable');
    return j;
}
export function entityFor(ref) {
    return ref ? saves.resolve(ref) : null;
}
export default class Housing {
    constructor(p) {
        this.p = p;
        if (!p.state.housing) {
            p.state.housing = {
                schema: 2,
                objects: {},
                containers: {},
                doors: {},
                boards: {},
                home: null,
                stats: {
                    placed: 0,
                    moved: 0,
                    transferred: 0,
                    barricaded: 0,
                    repaired: 0,
                },
            };
            for (const f of HOUSING.fixtures) {
                const e = world.entity(f.entity);
                const ref = saves.reference(e),
                    d = HOUSING.catalog[f.kind];
                this.state.objects[ref] = {
                    ref,
                    kind: f.kind,
                    owned: f.owned,
                    movable: f.movable,
                    support: saves.reference(world.entity(f.support)),
                    health: d.health,
                    maxHealth: d.health,
                    cost: d.cost,
                };
                if (d.storage) {
                    let bag = Bag.createBag(...d.storage);
                    for (const [id, n] of Object.entries(f.stock || {})) {
                        const r = Bag.add(bag, id, n);
                        if (!r.ok)
                            throw new Error(
                                'Invalid authored stock: ' + r.reason,
                            );
                        bag = r.bag;
                    }
                    this.state.containers[ref] = bag;
                }
            }
            for (const d of HOUSING.doors) {
                const leaf = world.entity(d.entity),
                    ref = saves.reference(leaf);
                this.state.doors[ref] = {
                    ref,
                    frame: saves.reference(world.entity(d.frame)),
                    anchorA: vec(d.anchorA),
                    anchorB: vec(d.anchorB),
                    opened: false,
                };
            }
        }
        for (const d of Object.values(this.state.doors)) this.ensureHinge(d);
    }
    get state() {
        return this.p.state.housing;
    }
    record(e) {
        const ref = e?.valid ? saves.reference(e) : null;
        return ref ? this.state.objects[ref] : null;
    }
    near(ref) {
        const e = entityFor(ref);
        return e?.valid &&
            length(
                sub(e.transform.position, this.p.entity.transform.position),
            ) < 5.5
            ? e
            : null;
    }
    ensureHinge(d) {
        const e = entityFor(d.ref),
            frame = entityFor(d.frame);
        if (!e?.valid || !frame?.valid) return null;
        return (
            physics.joint(e) ||
            physics.createJoint(e, {
                type: 'hinge',
                bodyA: e,
                bodyB: frame,
                anchorA: d.anchorA,
                anchorB: d.anchorB,
                frameA: hingeFrame,
                frameB: hingeFrame,
                enabled: true,
                limits: true,
                lower: -1.65,
                upper: 0,
                spring: true,
                rest: d.opened ? -1.5 : 0,
                stiffness: 350,
                damping: 70,
            })
        );
    }
    socket(e) {
        if (!e?.valid) return null;
        const ref = saves.reference(e),
            authored = HOUSING.sockets.find((s) => s.entity === e.id);
        if (authored)
            return {
                ref,
                support: e,
                position: vec(authored.position),
                rotation: {
                    w: authored.rotation[0],
                    x: authored.rotation[1],
                    y: authored.rotation[2],
                    z: authored.rotation[3],
                },
                door: authored.door
                    ? saves.reference(world.entity(authored.door))
                    : null,
                name: authored.name,
            };
        const r = this.state.objects[ref];
        if (!r || !['window_wall', 'door'].includes(r.kind)) return null;
        return {
            ref,
            support: e,
            position: add(
                e.transform.position,
                rotate(e.transform.rotation, {
                    x: 0,
                    y: r.kind === 'door' ? -0.1 : 0.15,
                    z: 0.22,
                }),
            ),
            rotation: e.transform.rotation,
            door: r.leaf || null,
            name: r.kind === 'door' ? 'Door opening' : 'Window opening',
        };
    }
    interact(hit) {
        const e = hit?.entity;
        if (!e?.valid) return false;
        const ref = saves.reference(e);
        if (this.state.doors[ref]) {
            this.toggleDoor(ref);
            return true;
        }
        if (this.record(e) || this.socket(e)) {
            this.p.hud.inspect(ref);
            return true;
        }
        return false;
    }
    context(hit) {
        const e = hit?.entity;
        if (!e?.valid) return '';
        const ref = saves.reference(e),
            r = this.record(e);
        if (this.state.doors[ref]) return 'E · Open / close physical door';
        if (r)
            return `E · ${HOUSING.catalog[r.kind]?.name || 'Barricade'} / actions${r.barricade ? ' · ' + r.health + '/' + r.maxHealth : ''}`;
        if (this.socket(e)) return 'E · Secure this door / window opening';
        return '';
    }
    toggleDoor(ref) {
        const d = this.state.doors[ref];
        if (!d || !this.near(ref)) return false;
        if (this.state.boards[d.frame]) {
            this.p.say(
                'The entrance is barricaded. Remove the barricade first.',
            );
            return false;
        }
        const hinge = this.ensureHinge(d);
        if (!hinge?.valid) {
            this.p.say('Door connection unavailable.');
            return false;
        }
        d.opened = !d.opened;
        hinge.setSpring(d.opened ? -1.5 : 0, 350, 70);
        this.p.say(d.opened ? 'Door opening.' : 'Door closing.');
        return true;
    }
    atStation(kind) {
        return Object.values(this.state.objects).some(
            (r) =>
                (r.kind === kind || HOUSING.catalog[r.kind]?.use === kind) &&
                this.near(r.ref) &&
                length(
                    sub(
                        entityFor(r.ref).transform.position,
                        this.p.entity.transform.position,
                    ),
                ) < 3,
        );
    }
    transfer(ref, slot, toStorage, whole = false) {
        if (!this.near(ref) || !this.state.containers[ref]) {
            this.p.say('Storage is no longer in reach.');
            return false;
        }
        const storage = this.state.containers[ref],
            from = toStorage ? this.p.state.bag : storage,
            to = toStorage ? storage : this.p.state.bag,
            item = from.slots[slot];
        if (!item) {
            this.p.say('Select an item first.');
            return false;
        }
        const r = Bag.transfer(from, to, item.item, whole ? item.count : 1);
        if (!r.ok) {
            this.p.say(r.reason);
            return false;
        }
        this.p.state.bag = toStorage ? r.from : r.to;
        this.state.containers[ref] = toStorage ? r.to : r.from;
        this.p.reconcileEquipment();
        this.state.stats.transferred++;
        this.p.say(toStorage ? 'Supplies stored.' : 'Supplies taken.');
        return true;
    }
    use(ref) {
        const e = this.near(ref),
            r = e && this.record(e);
        if (!r) return;
        if (this.state.containers[ref]) {
            this.p.hud.openStorage(ref);
            return;
        }
        if (HOUSING.catalog[r.kind]?.use === 'bed') {
            const v = this.p.state.vitals;
            if (v.hunger < 10 || v.thirst < 10) {
                this.p.say('Eat and drink before resting.');
                return;
            }
            v.hunger -= 5;
            v.thirst -= 5;
            v.stamina = 100;
            v.health = Math.min(100, v.health + 15);
            this.state.home = ref;
            this.p.say('Rested. This bed is now your recovery point.');
        } else if (HOUSING.catalog[r.kind]?.use === 'workbench') {
            this.p.hud.open('craft');
            this.p.say(
                'Workbench in reach: nails and metal sheets can be crafted here.',
            );
        } else if (HOUSING.catalog[r.kind]?.use === 'lamp') {
            e.setLight({ enabled: !e.light.enabled });
            this.p.say(e.light.enabled ? 'Lamp on.' : 'Lamp off.');
        }
    }
    homePosition() {
        const e = entityFor(this.state.home);
        if (!e?.valid) return null;
        return add(
            e.transform.position,
            rotate(e.transform.rotation, { x: 0, y: -0.2, z: 1.8 }),
        );
    }
    barricade(ref, metal = false) {
        const support = this.near(ref),
            socket = this.socket(support);
        if (!socket) {
            this.p.say('Select a valid opening.');
            return false;
        }
        if (this.state.boards[ref]) {
            this.p.say('That opening is already secured.');
            return false;
        }
        if (socket.door && this.state.doors[socket.door]?.opened) {
            this.p.say('Close the door before barricading it.');
            return false;
        }
        const cost = metal
                ? { metal_sheet: 1, nails: 2 }
                : { planks: 2, nails: 1 },
            debit = Bag.exchange(this.p.state.bag, cost, {});
        if (!debit.ok) {
            this.p.say(debit.reason);
            return false;
        }
        let e;
        try {
            e = world.spawnPrefab(
                HOUSING.prefabs['barricade_' + (metal ? 'metal' : 'wood')],
                { position: socket.position, rotation: socket.rotation },
            );
            connect(e, support, socket.position);
            const key = saves.reference(e);
            if (!key) throw new Error('Persistent identity unavailable');
            this.state.objects[key] = {
                ref: key,
                kind: metal ? 'barricade_metal' : 'barricade_wood',
                barricade: true,
                metal,
                owned: true,
                movable: false,
                support: ref,
                socket: ref,
                health: metal ? 120 : 60,
                maxHealth: metal ? 120 : 60,
                cost,
                reinforced: false,
            };
            this.state.boards[ref] = key;
            this.p.state.bag = debit.bag;
            this.state.stats.barricaded++;
            this.p.effect('craft', 'Opening physically barricaded.');
            return true;
        } catch (error) {
            if (e?.valid) e.destroy();
            this.p.say(
                'Barricade failed; materials retained: ' + error.message,
            );
            return false;
        }
    }
    damage(hit) {
        const r = this.record(hit?.entity);
        if (!r) return false;
        if (
            ['Furniture', 'Decoration'].includes(
                HOUSING.catalog[r.kind]?.category,
            )
        ) {
            this.p.say(
                'Use E to move or dismantle furnishings; empty storage first.',
            );
            return true;
        }
        if (!r.owned) {
            this.p.say('Use E to move or salvage furniture.');
            return true;
        }
        const tool = this.p.state.equipped;
        if (!['stone_axe', 'stone_pickaxe'].includes(tool)) {
            this.p.say('Use an axe or pick to damage constructed pieces.');
            return true;
        }
        const amount = r.metal && tool === 'stone_axe' ? 5 : 20;
        r.health = Math.max(0, r.health - amount);
        hit.entity.applyImpulseAtPoint(
            mul(this.p.camera.pose().direction, 20),
            hit.point,
        );
        this.p.effect('chop', 'Integrity ' + r.health + '/' + r.maxHealth);
        if (r.health === 0) this.remove(r.ref, false);
        return true;
    }
    repair(ref) {
        const r = this.state.objects[ref];
        if (!r || !this.near(ref) || r.health >= r.maxHealth) {
            this.p.say('No repair needed.');
            return false;
        }
        const cost = r.metal
                ? { metal_sheet: 1 }
                : r.barricade
                  ? { planks: 1 }
                  : { wood: 1 },
            debit = Bag.exchange(this.p.state.bag, cost, {});
        if (!debit.ok) {
            this.p.say(debit.reason);
            return false;
        }
        this.p.state.bag = debit.bag;
        r.health = r.maxHealth;
        this.state.stats.repaired++;
        this.p.effect('craft', 'Repaired to full integrity.');
        return true;
    }
    reinforce(ref) {
        const old = this.state.objects[ref],
            e = this.near(ref);
        if (!old?.barricade || old.reinforced || !e) return false;
        const cost = { metal_sheet: 1, nails: 2 },
            debit = Bag.exchange(this.p.state.bag, cost, {});
        if (!debit.ok) {
            this.p.say(debit.reason);
            return false;
        }
        let next;
        try {
            next = world.spawnPrefab(
                HOUSING.prefabs.barricade_metal,
                e.transform,
            );
            connect(next, entityFor(old.support), next.transform.position);
            const key = saves.reference(next);
            if (!key) throw new Error('Persistent identity unavailable');
            this.state.objects[key] = {
                ...old,
                ref: key,
                kind: 'barricade_metal',
                metal: true,
                reinforced: true,
                health: old.health + 80,
                maxHealth: old.maxHealth + 80,
                cost: {
                    ...old.cost,
                    metal_sheet: (old.cost.metal_sheet || 0) + 1,
                    nails: (old.cost.nails || 0) + 2,
                },
            };
            e.destroy();
            delete this.state.objects[ref];
            this.state.boards[old.socket] = key;
            this.p.state.bag = debit.bag;
            this.p.hud.objectRef = key;
            this.p.effect('craft', 'Metal reinforcement installed.');
            return true;
        } catch (error) {
            if (next?.valid) next.destroy();
            this.p.say(
                'Reinforcement failed; materials retained: ' + error.message,
            );
            return false;
        }
    }
    damageFromActor(hit, attacker, amount) {
        const r = this.record(hit?.entity);
        if (
            !r?.barricade ||
            !attacker?.valid ||
            length(sub(attacker.transform.position, hit.point)) > 2.3 ||
            !Number.isFinite(amount) ||
            amount <= 0
        )
            return false;
        r.health = Math.max(0, r.health - amount);
        if (r.health === 0) this.remove(r.ref, false, false);
        return true;
    }
    remove(ref, refund = true, requireNear = true) {
        const r = this.state.objects[ref],
            e = requireNear ? this.near(ref) : entityFor(ref);
        if (!r || !e?.valid) return false;
        if (this.state.containers[ref]?.slots.some(Boolean)) {
            this.p.say('Empty this storage before dismantling it.');
            return false;
        }
        let next = this.p.state.bag;
        if (refund) {
            const materials = Object.fromEntries(
                Object.entries(r.cost || {})
                    .map(([k, n]) => [k, Math.floor(n / 2)])
                    .filter(([, n]) => n > 0),
            );
            const result = Bag.exchange(next, {}, materials);
            if (!result.ok) {
                this.p.say(result.reason);
                return false;
            }
            next = result.bag;
        }
        // Removing a support explicitly releases its physical attachments. Bodies
        // retain their native mass and motion; gravity determines subsequent fall.
        for (const child of Object.values(this.state.objects))
            if (child.support === ref) {
                const target = entityFor(child.ref);
                if (target?.valid) physics.joint(target)?.destroy();
            }
        if (r.leaf) {
            delete this.state.doors[r.leaf];
        }
        e.destroy();
        delete this.state.objects[ref];
        delete this.state.containers[ref];
        if (r.socket) delete this.state.boards[r.socket];
        if (this.state.home === ref) this.state.home = null;
        this.p.state.bag = next;
        this.p.say(
            refund
                ? 'Dismantled; half the reusable materials recovered.'
                : 'The damaged piece broke apart.',
        );
        return true;
    }
}

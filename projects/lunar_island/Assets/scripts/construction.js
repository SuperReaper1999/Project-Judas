// Collision-aware player construction and furniture rearrangement. No native
// game systems: public prefabs, queries, rigid bodies and fixed/hinged joints.
import { world, physics, input, saves } from 'judas';
import { HOUSING } from './housing-data.js';
import * as Bag from './inventory.js';
import { connect, entityFor } from './housing.js';
import {
    add,
    sub,
    mul,
    length,
    norm,
    dot,
    vec,
    axis,
    fromTo,
    qm,
    rotate,
    inverse,
} from './math.js';
export default class Construction {
    constructor(p) {
        this.p = p;
        this.kind = null;
        this.moving = null;
        this.turn = 0;
        this.hide();
    }
    get active() {
        return !!this.kind;
    }
    hide() {
        for (const id of Object.values(HOUSING.previews)) {
            const e = world.entity(id);
            if (e?.valid) e.renderVisible = false;
        }
    }
    cancel() {
        this.hide();
        this.kind = null;
        this.moving = null;
    }
    begin(kind, ref = null) {
        if (!HOUSING.catalog[kind]) return;
        this.kind = kind;
        this.moving = ref;
        this.turn = 0;
        this.p.hud.close();
        this.p.blocked = true;
        this.p.say(
            'Aim at clear support. R rotates · Click confirms · Escape cancels.',
        );
    }
    move(ref) {
        const e = this.p.housing.near(ref),
            r = e && this.p.housing.record(e);
        if (!r?.movable) {
            this.p.say('This piece cannot be moved.');
            return;
        }
        this.begin(r.kind, ref);
    }
    candidate() {
        const hit = this.p.camera.hit(5),
            def = HOUSING.catalog[this.kind];
        if (!hit?.entity?.valid)
            return { error: 'Aim at a support within five metres.' };
        if (!hit.entity.hasTag('ground') && !hit.entity.hasTag('buildable'))
            return { error: 'Choose solid terrain or a structural support.' };
        const up = norm(mul(physics.gravity(hit.point), -1)),
            half = vec(def.half),
            supporting = this.p.housing.record(hit.entity),
            supportDef = supporting && HOUSING.catalog[supporting.kind];
        let point = hit.point,
            rotation = qm(
                fromTo({ x: 0, y: 1, z: 0 }, up),
                axis({ x: 0, y: 1, z: 0 }, this.turn),
            ),
            position;
        const wallMounted = def.surface === 'wall';
        if (this.kind === 'foundation' && !hit.entity.hasTag('ground'))
            return { error: 'Foundations need solid terrain.' };
        if (
            ['Structure', 'Garden'].includes(def.category) &&
            supportDef?.category === 'Furniture'
        )
            return { error: 'Use terrain or a structural piece as support.' };
        if (
            this.kind === 'roof' &&
            ['wall', 'window_wall'].includes(supporting?.kind)
        ) {
            const t = hit.entity.transform,
                normal = rotate(t.rotation, { x: 0, y: 0, z: 1 });
            if (
                Math.abs(dot(normal, hit.normal)) < 0.7 ||
                dot(rotate(t.rotation, { x: 0, y: 1, z: 0 }), up) < 0.8
            )
                return {
                    error: 'Aim at the broad face of an upright wall to fit the roof.',
                };
            point = add(
                t.position,
                rotate(t.rotation, { x: 0, y: supportDef.half[1], z: 0 }),
            );
            position = add(
                sub(point, mul(hit.normal, half.z - supportDef.half[2])),
                mul(up, half.y + 0.018),
            );
            rotation = t.rotation;
        } else if (
            ['wall', 'window_wall', 'door', 'fence', 'gate'].includes(
                this.kind,
            ) &&
            ['floor', 'foundation'].includes(supporting?.kind)
        ) {
            const t = hit.entity.transform,
                local = rotate(inverse(t.rotation), sub(hit.point, t.position)),
                quarter = Math.round(this.turn / (Math.PI / 2)) % 2;
            const edge = quarter
                ? {
                      x: (local.x < 0 ? -1 : 1) * (supportDef.half[0] - 0.12),
                      y: supportDef.half[1],
                      z: 0,
                  }
                : {
                      x: 0,
                      y: supportDef.half[1],
                      z: (local.z < 0 ? -1 : 1) * (supportDef.half[2] - 0.12),
                  };
            point = add(t.position, rotate(t.rotation, edge));
            rotation = qm(t.rotation, axis({ x: 0, y: 1, z: 0 }, this.turn));
            position = add(point, mul(up, half.y + 0.018));
        } else {
            if (
                wallMounted
                    ? Math.abs(dot(hit.normal, up)) > 0.3
                    : dot(hit.normal, up) < 0.65
            )
                return {
                    error: wallMounted
                        ? 'Paintings need a vertical wall.'
                        : 'Aim at a ground or floor top. Roofs also snap to wall faces.',
                };
            if (wallMounted)
                rotation = fromTo({ x: 0, y: 0, z: 1 }, hit.normal);
            position = add(
                point,
                mul(
                    wallMounted ? hit.normal : up,
                    (wallMounted ? half.z : half.y) + 0.018,
                ),
            );
        }
        const ignored = [hit.entity],
            old = entityFor(this.moving);
        if (old?.valid) ignored.push(old);
        const obstruction = physics.boxCast(
            { position, rotation },
            mul(half, 0.94),
            up,
            0,
            { ignored },
        );
        if (obstruction)
            return {
                error:
                    'Blocked by ' +
                    (obstruction.entity?.name || 'another object') +
                    '. Choose a clear space.',
            };
        return { position, rotation, support: hit.entity, point };
    }

    confirm() {
        const c = this.candidate();
        if (c.error) {
            this.p.say(c.error);
            return false;
        }
        const d = HOUSING.catalog[this.kind];
        if (this.moving) return this.moveConfirm(c);
        const h = this.p.housing.state;
        if (Object.values(h.objects).filter((r) => r.owned).length >= 128) {
            this.p.say(
                '128 constructed pieces maximum. Dismantle a piece first.',
            );
            return false;
        }
        const debit = Bag.exchange(this.p.state.bag, d.cost, {});
        if (!debit.ok) {
            this.p.say(debit.reason);
            return false;
        }
        let e,
            pendingLeafRef = null;
        try {
            e = world.spawnPrefab(HOUSING.prefabs[this.kind], {
                position: c.position,
                rotation: c.rotation,
            });
            connect(e, c.support, c.point);
            const ref = saves.reference(e);
            if (!ref) throw new Error('Persistent identity unavailable');
            const record = {
                ref,
                kind: this.kind,
                owned: true,
                movable: ![
                    'door',
                    'gate',
                    'wall',
                    'window_wall',
                    'foundation',
                    'floor',
                    'roof',
                    'fence',
                ].includes(this.kind),
                support: saves.reference(c.support),
                health: d.health,
                maxHealth: d.health,
                cost: d.cost,
            };
            if (['door', 'gate'].includes(this.kind)) {
                const leaf = e.children[0];
                if (!leaf?.valid) throw new Error('Door leaf missing');
                const leafRef = saves.reference(leaf);
                pendingLeafRef = leafRef;
                record.leaf = leafRef;
                h.doors[leafRef] = {
                    ref: leafRef,
                    frame: ref,
                    anchorA: { x: -0.65, y: 0, z: -0.075 },
                    anchorB: { x: -0.65, y: -0.15, z: -0.075 },
                    opened: false,
                };
                if (!this.p.housing.ensureHinge(h.doors[leafRef])?.valid)
                    throw new Error('Hinge creation failed');
            }
            h.objects[ref] = record;
            if (d.storage) h.containers[ref] = Bag.createBag(...d.storage);
            this.p.state.bag = debit.bag;
            h.stats.placed++;
            this.p.effect(
                'craft',
                'Placed ' +
                    d.name +
                    '. A real physical joint connects it to its support.',
            );
            this.cancel();
            return true;
        } catch (error) {
            if (pendingLeafRef) delete h.doors[pendingLeafRef];
            if (e?.valid) e.destroy();
            this.p.say(
                'Placement failed; materials retained: ' + error.message,
            );
            return false;
        }
    }
    moveConfirm(c) {
        const ref = this.moving,
            e = this.p.housing.near(ref),
            r = e && this.p.housing.record(e);
        if (!r) {
            this.cancel();
            return false;
        }
        const before = e.transform,
            oldMotion = e.motionType,
            oldVelocity =
                oldMotion === 'dynamic' ? e.velocity : { x: 0, y: 0, z: 0 },
            oldAngular =
                oldMotion === 'dynamic'
                    ? e.angularVelocity
                    : { x: 0, y: 0, z: 0 };
        try {
            if (oldMotion !== 'dynamic')
                e.setMotionType('dynamic', { preserveVelocity: false });
            e.transform = { position: c.position, rotation: c.rotation };
            e.velocity = { x: 0, y: 0, z: 0 };
            e.angularVelocity = { x: 0, y: 0, z: 0 };
            connect(e, c.support, c.point);
            r.support = saves.reference(c.support);
            this.p.housing.state.stats.moved++;
            this.p.say(
                'Furniture repositioned. Its storage and identity are unchanged.',
            );
            this.cancel();
            return true;
        } catch (error) {
            e.transform = before;
            e.setMotionType(oldMotion, { preserveVelocity: false });
            if (oldMotion === 'dynamic') {
                e.velocity = oldVelocity;
                e.angularVelocity = oldAngular;
            }
            this.p.say('Move cancelled: ' + error.message);
            return false;
        }
    }
    tick() {
        if (this.active && input.pressed('build_rotate'))
            this.turn += Math.PI / 2;
    }
    present() {
        this.hide();
        if (!this.active || this.p.hud.modal) return;
        const c = this.candidate();
        if (c.error) {
            this.p.hud.doc.get('context').text = c.error;
            return;
        }
        const e = world.entity(HOUSING.previews[this.kind]);
        e.transform = { position: c.position, rotation: c.rotation };
        e.renderVisible = true;
        this.p.hud.doc.get('context').text =
            'Click place · R rotate · Escape cancel';
    }
}

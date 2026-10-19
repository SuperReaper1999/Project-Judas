import { world, physics, saves, input } from 'judas';
import { WORLD } from './world-data.js';
import { recipes, kinds, afford, debit } from './inventory.js';
import { add, sub, mul, dot, length, norm, rotate, inverse, qm, axis, fromTo, vec } from './math.js';
const identity = { w: 1, x: 0, y: 0, z: 0 };
export default class Builder {
    constructor(player) { this.p = player; this.preview = null; }
    candidate(hit) {
        if (!hit?.entity?.valid)
            return null;
        const support = hit.entity, s = this.p.state, kind = s.selectedBuild;
        if (!support.hasTag('ground') && !support.hasTag('buildable'))
            return null;
        const up = norm(mul(physics.gravity(hit.point), -1));
        if (length(up) < .9 || dot(hit.normal, up) < .65)
            return null;
        const rotation = qm(fromTo({ x: 0, y: 1, z: 0 }, up), axis({ x: 0, y: 1, z: 0 }, s.buildRotation));
        const half = vec(WORLD.buildHalfExtents[kind]), position = add(hit.point, mul(up, half.y + .025));
        // A slightly inset physical shape detects actual obstruction, independent of
        // conservative broadphase bounds. Only the intended supporting body is ignored.
        const obstruction = physics.boxCast({ position, rotation }, mul(half, .94), up, 0, { ignored: [this.p.entity, support] });
        const own = add(this.p.entity.transform.position, mul(this.p.entity.character.up, .85));
        if (obstruction || length(sub(own, position)) < length(half) + .7)
            return null;
        return { support, position, rotation, point: hit.point, kind };
    }
    place(hit) {
        const s = this.p.state, kind = s.selectedBuild;
        if (!afford(s, kind)) {
            const r = recipes[kind];
            this.p.say('Need ' + r.wood + ' wood + ' + r.stone + ' stone for ' + kind + '.');
            return;
        }
        if (s.built.length >= 64) {
            this.p.say('Homestead limit: 64 pieces. Dismantle a piece before building more.');
            return;
        }
        const c = this.candidate(hit);
        if (!c) {
            this.p.say('Aim at a clear, walkable ground or floor surface, away from your feet.');
            return;
        }
        let e = null;
        try {
            e = world.spawnPrefab(WORLD.prefabs[kind], { position: c.position, rotation: c.rotation });
            const b = c.support.transform;
            const joint = physics.createJoint(e, { type: 'fixed', bodyA: e, bodyB: c.support, anchorA: rotate(inverse(c.rotation), sub(c.point, c.position)), anchorB: rotate(inverse(b.rotation), sub(c.point, b.position)), frameA: identity, frameB: qm(inverse(b.rotation), c.rotation), enabled: true });
            if (!joint?.valid)
                throw new Error('Connection unavailable');
            const reference = saves.reference(e);
            if (!reference)
                throw new Error('Persistent identity unavailable');
            s.built.push({ reference, kind, support: saves.reference(c.support), contact: c.point });
            debit(s, kind);
            s.stats.placed++;
            this.p.feedback('Built ' + kind + ' — fixed physical connection', 'build', c.point);
        }
        catch (error) {
            if (e?.valid)
                e.destroy();
            this.p.say('Placement failed; resources retained: ' + error.message);
        }
    }
    dismantle(hit) {
        if (!hit)
            return;
        const s = this.p.state, index = s.built.findIndex(b => saves.resolve(b.reference)?.id === hit.entity.id);
        if (index < 0) {
            this.p.say('Only your constructed pieces can be dismantled.');
            return;
        }
        const item = s.built[index], cost = recipes[item.kind];
        hit.entity.destroy();
        s.built.splice(index, 1);
        s.inventory.wood += cost.wood;
        s.inventory.stone += cost.stone;
        s.stats.woodSpent -= cost.wood;
        s.stats.stoneSpent -= cost.stone;
        this.p.feedback('Dismantled ' + item.kind + '. Attached pieces may fall.', 'build', hit.point);
    }
    support() {
        const terrain = world.entity(WORLD.terrain), state = terrain.terrain.state;
        if (!state?.ready || state.busy || state.revision === this.revision)
            return;
        this.revision = state.revision;
        // A terrain recook keeps the body's identity. Recheck the actual anchor after
        // publication so excavating it releases the joint instead of levitating it.
        for (const item of this.p.state.built) {
            const e = saves.resolve(item.reference), support = saves.resolve(item.support);
            if (!e?.valid || support?.id !== WORLD.terrain || !item.contact)
                continue;
            const joint = physics.joint(e);
            if (!joint?.valid)
                continue;
            const up = norm(mul(physics.gravity(item.contact), -1)), hit = physics.raycast(add(item.contact, mul(up, .18)), mul(up, -1), .42, { ignored: [e, this.p.entity], requiredTags: ['ground'] });
            if (!hit || hit.entity?.id !== WORLD.terrain) {
                joint.destroy();
                this.p.say('Excavation removed a foundation: the connected structure is now free to fall.');
            }
        }
    }
    tick() { this.support(); if (this.p.state.equipped !== 'build')
        return; if (input.pressed('build_next'))
        this.p.state.selectedBuild = kinds[(kinds.indexOf(this.p.state.selectedBuild) + 1) % kinds.length]; if (input.pressed('build_rotate'))
        this.p.state.buildRotation += Math.PI / 2; if (!this.p.blocked && input.pressed('grab'))
        this.dismantle(this.p.camera.hit(5)); }
    present() {
        for (const [kind, id] of Object.entries(WORLD.previews || {})) {
            const e = world.entity(id);
            if (!e?.valid)
                continue;
            e.renderVisible = false;
        }
        if (this.p.hud.modal || this.p.state.equipped !== 'build')
            return;
        const c = this.candidate(this.p.camera.hit(5));
        if (!c)
            return;
        const e = world.entity(WORLD.previews?.[c.kind]);
        if (e?.valid) {
            e.transform = { position: c.position, rotation: c.rotation };
            e.renderVisible = true;
        }
    }
}

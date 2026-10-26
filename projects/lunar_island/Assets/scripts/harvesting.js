import { world } from 'judas';
import { WORLD } from './world-data.js';
import { regionalResources } from './region-session.js';
import { add as insert } from './inventory.js';
import { add, mul, norm } from './math.js';
export function nodeFor(hit) {
    return hit?.entity
        ? WORLD.resources.find((n) => n.id === hit.entity.id) ||
              regionalResources.get(hit.entity.id)
        : null;
}
export const harvestStatus = (p, n) =>
    (n.owner?.state.harvest || p.state.harvest)[n.id];
export function collect(p, hit) {
    if (!hit?.entity?.valid) return false;
    const e = hit.entity,
        drop = p.state.drops[e.id];
    if (drop) {
        const r = insert(p.state.bag, drop.item, drop.count);
        if (!r.ok) {
            p.say(r.reason);
            return true;
        }
        p.state.bag = r.bag;
        delete p.state.drops[e.id];
        e.destroy();
        p.effect('gather', 'Recovered dropped supplies.');
        return true;
    }
    const n = nodeFor(hit);
    if (!n || ['tree', 'rock', 'ore'].includes(n.kind)) return false;
    const item = {
        branch: 'wood',
        loose_stone: 'stone',
        berries: 'berries',
        fiber: 'fiber',
        salvage: 'scrap',
    }[n.kind];
    if (!item) return false;
    const r = insert(p.state.bag, item, n.amount);
    if (!r.ok) {
        p.say(r.reason);
        return true;
    }
    p.state.bag = r.bag;
    (n.owner?.state.harvest || p.state.harvest)[n.id] = { depleted: true };
    e.destroy();
    p.state.stats.gathered += n.amount;
    p.effect('gather', 'Gathered ' + n.amount + ' ' + item + '.');
    return true;
}
export function useTool(p, hit) {
    const n = nodeFor(hit);
    if (!n || !hit.entity.valid) {
        p.say(
            'Aim at a trunk or stone deposit within reach. E gathers loose materials.',
        );
        return;
    }
    const r =
        (n.owner?.state.harvest || p.state.harvest)[n.id] ||
        ((n.owner?.state.harvest || p.state.harvest)[n.id] = {
            hits: 0,
            felled: false,
            processed: 0,
        });
    if (r.depleted) return;
    const required =
        n.kind === 'tree'
            ? 'stone_axe'
            : ['ore', 'rock'].includes(n.kind)
              ? 'stone_pickaxe'
              : '';
    if (!required || p.state.equipped !== required) {
        p.say(
            n.kind === 'tree'
                ? 'Craft and equip a stone axe.'
                : 'Craft and equip a stone pickaxe.',
        );
        return;
    }
    if (n.kind === 'tree') {
        if (!r.felled) {
            r.hits++;
            p.effect('chop', 'Tree trunk · ' + r.hits + '/3 cuts');
            if (r.hits === 3) {
                const e = hit.entity;
                e.setMotionType('dynamic', { preserveVelocity: true });
                const direction = norm({ ...p.camera.pose().direction, y: 0 });
                e.applyImpulseAtPoint(
                    mul(direction, 150),
                    add(e.transform.position, {
                        x: 0,
                        y: n.height * 0.32,
                        z: 0,
                    }),
                );
                r.felled = true;
                p.state.stats.felled++;
                p.say(
                    'Timber! Chop the fallen trunk again to collect usable wood.',
                );
            }
        } else {
            r.processed++;
            if (r.processed < 3) {
                p.effect(
                    'chop',
                    'Processing fallen timber · ' + r.processed + '/3',
                );
                return;
            }
            const result = insert(p.state.bag, 'wood', 12);
            if (!result.ok) {
                r.processed = 2;
                p.say(result.reason);
                return;
            }
            p.state.bag = result.bag;
            r.depleted = true;
            hit.entity.destroy();
            p.state.stats.timber += 12;
            p.effect('gather', 'Processed timber · +12 wood');
        }
    } else {
        const item = n.kind === 'ore' ? 'iron_ore' : 'stone',
            result = insert(p.state.bag, item, 1);
        if (!result.ok) {
            p.say(result.reason);
            return;
        }
        p.state.bag = result.bag;
        r.hits++;
        p.state.stats.mined++;
        p.effect(
            'chop',
            'Mined ' +
                item +
                ' · ' +
                Math.max(0, n.amount - r.hits) +
                ' remaining',
        );
        if (r.hits >= n.amount) {
            r.depleted = true;
            hit.entity.destroy();
        }
    }
}

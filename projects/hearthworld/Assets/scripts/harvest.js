import { world } from 'judas';
import { WORLD } from './world-data.js';
import { add, mul, dot, norm, rotate, vec } from './math.js';
import { collect } from './inventory.js';
export default class Harvest {
    constructor(player) { this.p = player; }
    record(id) { return this.p.state.harvest[id] || (this.p.state.harvest[id] = { hits: 0, felled: false, processed: false }); }
    interact(hit) {
        if (!hit)
            return false;
        const e = hit.entity, s = this.p.state, tool = WORLD.tools.find(t => t.id === e.id);
        if (tool) {
            s.owned[tool.kind] = true;
            e.destroy();
            s.equipped = tool.kind;
            this.p.feedback('Collected ' + tool.kind + '. Left click to use it.', 'pickup', hit.point);
            return true;
        }
        if (e.hasTag('wood')) {
            if (this.p.grab.held?.id === e.id)
                this.p.grab.drop();
            e.destroy();
            collect(s, 'wood', 3);
            this.p.feedback('+3 WOOD', 'pickup', hit.point);
            return true;
        }
        return false;
    }
    use(hit) {
        if (!hit) {
            this.p.say('Aim at a tree or rock within reach.');
            return;
        }
        const e = hit.entity, s = this.p.state, tree = WORLD.trees.find(t => t.id === e.id), rock = WORLD.rocks.find(r => r.id === e.id);
        if (s.equipped === 'axe' && tree) {
            const r = this.record(e.id);
            if (r.processed)
                return;
            if (!r.felled) {
                r.hits++;
                this.p.feedback('CHOP ' + r.hits + '/3', 'chop', hit.point);
                if (r.hits >= 3) {
                    if (!e.setMotionType('dynamic')) {
                        this.p.say('Tree body could not be released.');
                        r.hits--;
                        return;
                    }
                    const up = norm(vec(tree.base)), away = norm(add(mul(this.p.camera.pose().direction, 1), mul(up, -dot(this.p.camera.pose().direction, up))));
                    e.applyImpulseAtPoint(mul(away, 95), add(e.transform.position, mul(up, 2.5)));
                    r.felled = true;
                    r.processHits = 0;
                    s.stats.treesFelled++;
                    this.p.say('TIMBER! Let it fall, then chop the fallen trunk twice.');
                }
            }
            else {
                const lean = dot(rotate(e.transform.rotation, { x: 0, y: 1, z: 0 }), norm(vec(tree.base)));
                if (lean > .68) {
                    this.p.say('Stand clear. Wait for the trunk to fall before processing.');
                    return;
                }
                r.processHits = (r.processHits || 0) + 1;
                this.p.feedback('Process fallen trunk ' + r.processHits + '/2', 'chop', hit.point);
                if (r.processHits >= 2) {
                    const t = e.transform, angularVelocity = e.angularVelocity, spawned = [];
                    try {
                        for (let i = 0; i < 3; i++) {
                            const position = add(t.position, rotate(t.rotation, { x: 0, y: (i - 1) * 2.25, z: 0 }));
                            const log = world.spawnPrefab(WORLD.prefabs.log, { position, rotation: t.rotation });
                            log.velocity = e.pointVelocity(position);
                            log.angularVelocity = angularVelocity;
                            spawned.push(log);
                        }
                    }
                    catch (error) {
                        for (const log of spawned)
                            if (log.valid)
                                log.destroy();
                        r.processHits--;
                        this.p.say('Log preparation failed: ' + error.message);
                        return;
                    }
                    e.destroy();
                    r.processed = true;
                    s.stats.treesProcessed++;
                    this.p.say('Three physical logs. E collects each for 3 wood, or carry them.');
                }
            }
            return;
        }
        if (s.equipped === 'pickaxe' && rock) {
            const r = this.record(e.id);
            if (r.processed)
                return;
            r.hits++;
            collect(s, 'stone', 1);
            this.p.feedback('+1 STONE  /  ' + Math.max(0, 6 - r.hits) + ' left', 'stone', hit.point);
            if (r.hits >= 6) {
                r.processed = true;
                e.destroy();
                this.p.say('Deposit exhausted. Explore the planet for more stone.');
            }
            return;
        }
        this.p.say(tree ? 'Use the AXE for trees.' : rock ? 'Use the PICKAXE for stone.' : 'This tool does not harvest that object.');
    }
}

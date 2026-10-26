import { saves, scenes } from 'judas';
import { REGIONS } from './region-data.js';
import { housingRegions, emptyDistrictHousing } from './regional-housing.js';
import { regionalResources } from './region-session.js';
import { getPlayer } from './people-session.js';
export const properties = { key: { type: 'string', default: 'forest' } };
export default class RegionContent {
    constructor({ entity, properties }) {
        this.entity = entity;
        this.key = properties.key;
        this.state = { housing: emptyDistrictHousing(), harvest: {} };
        this.bound = [];
        this.ready = false;
    }
    start() {
        this.register();
    }
    restore() {
        this.register();
    }
    register() {
        const p = getPlayer();
        if (!p?.housing) return;
        const d = REGIONS[this.key];
        if (!d) throw new Error('Unknown district ' + this.key);
        for (const id of this.bound) regionalResources.delete(id);
        this.bound = [];
        housingRegions.set(this.key, this);
        p.housing.register(d.fixtures, d.doors, d.sockets);
        for (const n of d.resources) {
            const e = saves.resolve(n.id);
            if (!e?.valid) continue; // Depleted entities are legitimately absent.
            if (this.state.harvest[n.id]?.depleted) {
                e.destroy();
                continue;
            }
            regionalResources.set(e.id, { ...n, owner: this });
            this.bound.push(e.id);
        }
        this.ready = true;
    }
    fixedUpdate() {
        if (!this.ready) this.register();
    }
    destroy() {
        for (const id of this.bound) regionalResources.delete(id);
        if (housingRegions.get(this.key) === this)
            housingRegions.delete(this.key);
    }
}

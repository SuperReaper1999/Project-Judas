import { crates } from './registry.js';
export default class PersistentCrate {
    constructor({ entity }) {
        this.entity = entity;
        this.state = { pushes: 0 };
    }
    start() {
        crates.set(this.entity.id, this);
    }
    restore() {
        crates.set(this.entity.id, this);
    }
    push(direction) {
        this.entity.applyImpulse({
            x: direction.x * 22,
            y: 5,
            z: direction.z * 22,
        });
        this.state.pushes++;
    }
    destroy() {
        if (crates.get(this.entity.id) === this) crates.delete(this.entity.id);
    }
}

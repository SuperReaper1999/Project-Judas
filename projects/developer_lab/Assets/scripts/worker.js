import { input, console } from 'judas';
import { calibratedDistance } from './calibration.js';
export default class Worker {
    constructor({ entity }) { this.entity = entity; this.state = { phase: 0, calibrated: false, samples: 0 }; }
    start() { console.log('Worker started; JSON phase is retained on Keep JSON state reload.'); }
    update(dt) {
        this.state.phase += dt;
        if (input.pressed('repair_test')) this.state.calibrated = true;
        const phase = this.state.phase;
        const distance = this.state.calibrated ? calibratedDistance(phase) : Math.sin(phase) * .35;
        this.state.samples++;
        this.entity.transform = { position: { x: distance, y: 1.9, z: 0 } };
    }
    destroy() { console.log('Worker retired. No physical world reset.'); }
}

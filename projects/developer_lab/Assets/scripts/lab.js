import { world, ui, input, scenes, entity } from 'judas';
export default class Workshop {
    constructor() { this.state = { elapsed: 0 }; }
    start() { this.doc = ui.get('workshop'); this.doc.modal = false; this.doc.get('pause').visible = false; ui.debugOverlayVisible = false; input.pointerCapture = false; }
    pause(on) { this.doc.modal = on; this.doc.get('pause').visible = on; }
    uiUpdate() { this.opened = false; if (input.pressed('pause') && !this.doc.modal) { this.pause(true); this.opened = true; } }
    onUI(e) { if (e.document !== 'workshop') return; if ((e.type === 'back' && !this.opened) || (e.type === 'click' && e.element === 'resume')) this.pause(false); if (e.type === 'click' && e.element === 'reload') scenes.reload(); }
    update(dt) {
        this.state.elapsed += dt;
        if (input.pressed('reset')) scenes.reload();
        const worker = entity('30').scriptState(1);
        this.doc.get('status').text = worker
            ? `WORKER / ${worker.calibrated ? 'CALIBRATED' : 'SAFE PREVIEW'} / samples ${worker.samples} / phase ${worker.phase.toFixed(2)}\nE tests the calibration. Deliberate gain:0 fault is isolated to the amber assembly.\nRepair calibration.js gain to 2. Fresh reload recovers faults; repeat E to verify the fix.`
            : 'WORKER FAULTED / presentation and UI remain live\nOpen developer tools > Runtime errors. Fix calibration.js gain:2; fresh reload.\nKeep JSON state deliberately rejects a faulted instance. Stop/Play restores save compatibility.';
    }
    presentationUpdate() { world.setView({ position: { x: 0, y: 4.2, z: 9 }, rotation: { w: .987, x: -.16, y: 0, z: 0 } }, 55, { near: .1, far: 100 }); }
    destroy() { world.clearView(); }
}

import {world, entity, input, ui, scenes, saves, players} from 'judas';
import {add, sub, mul, norm, cross, look} from './math.js';

export const properties = {caseKey: {type: 'string', default: 'workshop'}};
const v = (x = 0, y = 0, z = 0) => ({x, y, z});
const CASES = ['workshop', 'stress', 'rotated'];

export default class SurfaceWorkshop {
    constructor({properties}) {
        this.kind = CASES.includes(properties.caseKey) ? properties.caseKey : 'workshop';
        this.state = {time: 0, tour: true, paused: false, cells: {}, receivers: {}, count: 0, weight: 0,
            eye: v(0, 14, 25), yaw: 0, pitch: -.35, viewId: null};
        this.statusAge = 1;
        this.request = null;
        this.savePending = false;
    }
    start() { this.bind(); }
    restore() { this.bind(); }
    bind() {
        this.doc = ui.get('particle_lab');
        this.doc.modal = false;
        this.doc.get('pause').visible = false;

        this.emitters = world.queryTags(['emitter']);
        this.receivers = world.queryTags(['receiver']);
        this.view = this.state.viewId ? players.get(this.state.viewId) : null;
        if (!this.view?.valid) {
            this.view = players.create('Workshop observer');
            this.state.viewId = this.view.id;
        }
        const rotor = entity('201');
        if (rotor?.valid) rotor.setKinematicVelocity(v(), this.kind === 'rotated' ? v(-.6, 0, 0) : v(0, .6, 0));
        for (const receiver of this.receivers) this.applyCoating(receiver.id);
        for (const emitter of this.emitters) if (this.state.paused) emitter.controlParticles('pause');
        input.pointerCapture = false;
        this.statusAge = 1;
    }
    applyCoating(id) {
        const receiver = entity(id);
        if (!receiver?.valid) return;
        const weight = this.state.receivers[id]?.weight || 0;
        const amount = 1 - Math.exp(-weight / 8);
        receiver.material().set({baseColor: {x: .22 - .12 * amount,
            y: .27 + .22 * amount, z: .34 + .56 * amount, a: 1},
            roughness: .85 - .5 * amount, doubleSided: true});
        // Paint is deliberately slippery: gameplay-owned deposition changes
        // the actual existing body's contact material, not merely its colour.
        receiver.setPhysicalMaterial(null, {friction: .75 - .6 * amount, restitution: .1});
        if (id === '100') {
            const weights = new Array(64).fill(0);
            for (const cell of Object.values(this.state.cells)) {
                if (cell.receiver !== id) continue;
                const x = Math.floor(cell.localPosition.x + 4), z = Math.floor(cell.localPosition.z + 4);
                if (x >= 0 && x < 8 && z >= 0 && z < 8) weights[z * 8 + x] += cell.weight;
            }
            for (let i = 0; i < weights.length; ++i) {
                const tile = entity(String(500 + i));
                if (!tile?.valid) continue;
                const coverage = 1 - Math.exp(-weights[i] / 2);
                tile.material().set({baseColor: {x: .22 - .12 * coverage,
                    y: .27 + .22 * coverage, z: .34 + .56 * coverage, a: 1},
                    roughness: .85 - .5 * coverage});
            }
        }
    }
    drainImpacts() {
        const dirty = {};
        for (const emitter of this.emitters) {
            for (const impact of emitter.takeParticleImpacts()) {
                const id = impact.receiver;
                if (!id || id === '0') continue;
                const p = impact.localPosition, n = impact.localNormal;
                const axis = Math.abs(n.x) > Math.abs(n.y) ? 0 : 1;
                const normalAxis = Math.abs(n.z) > Math.abs(axis === 0 ? n.x : n.y) ? 2 : axis;
                const key = [id, impact.childKey, normalAxis, Math.sign([n.x, n.y, n.z][normalAxis]),
                    Math.floor(p.x * 2), Math.floor(p.y * 2), Math.floor(p.z * 2)].join(':');
                let cell = this.state.cells[key];
                if (!cell) {
                    // At the explicit content budget retain all aggregate mass
                    // in an overflow receiver cell instead of losing events.
                    const chosen = Object.keys(this.state.cells).length < 4096 ? key : id + ':overflow';
                    cell = this.state.cells[chosen] || (this.state.cells[chosen] = {receiver: id,
                        localPosition: p, count: 0, weight: 0});
                }
                cell.count += impact.count;
                cell.weight += impact.weight;
                const total = this.state.receivers[id] || (this.state.receivers[id] = {count: 0, weight: 0});
                total.count += impact.count;
                total.weight += impact.weight;
                this.state.count += impact.count;
                this.state.weight += impact.weight;
                dirty[id] = true;
            }
        }
        for (const id of Object.keys(dirty)) this.applyCoating(id);
    }
    setPaused(paused) {
        this.state.paused = paused;
        for (const emitter of this.emitters) emitter.controlParticles(paused ? 'pause' : 'play');
    }
    clean() {
        this.drainImpacts();
        this.state.cells = {};
        this.state.receivers = {};
        this.state.count = this.state.weight = 0;
        for (const receiver of this.receivers) this.applyCoating(receiver.id);
    }
    pause(open) {
        this.doc.modal = open;
        this.doc.get('pause').visible = open;
        input.pointerCapture = !open && !this.state.tour;
    }
    uiUpdate() {
        this.openedPause = false;
        if (input.pressed('pause') && !this.doc.modal) {
            this.pause(true);
            this.openedPause = true;
        }
    }
    onUI(event) {
        if (event.document !== 'particle_lab') return;
        if ((event.type === 'back' && !this.openedPause) ||
            (event.type === 'click' && event.element === 'resume')) this.pause(false);
        if (event.type !== 'click') return;
        if (event.element === 'reload') scenes.reload();
        if (event.element.startsWith('case_')) this.openCase(Number(event.element.slice(5)) - 1);
    }
    openCase(index) {
        if (index >= 0 && index < CASES.length && CASES[index] !== this.kind)
            scenes.load('Scenes/' + CASES[index] + '.judas');
    }
    update(dt) {
        if (this.doc.modal) return;
        this.drainImpacts();
        this.state.time += dt;
        for (let i = 0; i < CASES.length; ++i)
            if (input.pressed('case_' + (i + 1))) { this.openCase(i); return; }
        if (input.pressed('reset')) scenes.reload();
        if (!this.savePending && input.pressed('emission')) this.setPaused(!this.state.paused);
        if (!this.savePending && input.pressed('burst'))
            for (const emitter of this.emitters) emitter.burst(this.kind === 'stress' ? 1000 : 30);
        if (input.pressed('clean')) this.clean();
        if (input.pressed('save')) {
            this.savePending = true;
            this.state.paused = false;
            // An already captured reliable tick must finish before it can be
            // saved. Resume that work while stopping future emission; pausing
            // immediately would also pause its backpressure recovery.
            for (const emitter of this.emitters) {
                emitter.controlParticles('play');
                emitter.controlParticles('stop');
            }
        }
        if (this.savePending && this.emitters.every(emitter => !emitter.particleStats?.pendingStep)) {
            this.drainImpacts();
            this.setPaused(true);
            this.savePending = false;
            this.request = saves.save('particle-v2-workshop', {name: 'Surface coating / ' + this.kind});
        }
        if (!this.savePending && input.pressed('load')) this.request = saves.load('particle-v2-workshop');
        if (input.pressed('watch')) {
            this.state.tour = !this.state.tour;
            input.pointerCapture = !this.state.tour;
        }
        if (!this.state.tour) {
            this.state.yaw -= input.axis('look_x') * .002;
            this.state.pitch = Math.max(-1.4, Math.min(1.4, this.state.pitch - input.axis('look_y') * .002));
            const f = v(-Math.sin(this.state.yaw), 0, -Math.cos(this.state.yaw));
            const direction = add(add(mul(f, input.axis('move_z')),
                mul(cross(f, v(0, 1, 0)), input.axis('move_x'))), v(0, input.axis('move_y'), 0));
            this.state.eye = add(this.state.eye, mul(norm(direction), dt * 12));
        }
        this.statusAge += dt;
        if (this.statusAge < .25) return;
        this.statusAge = 0;
        const stats = this.emitters.map(e => e.particleStats).filter(Boolean);
        const sum = key => stats.reduce((n, s) => n + (s[key] || 0), 0);
        let text = this.kind === 'stress' ? '100,000 visual particles requested explicitly. GPU live counts arrive asynchronously.' :
            'Actual impacts paint receiver-local cells; coating lowers real body friction from 0.75 toward 0.15.';
        text += '\nBackend: ' + stats.map(s => s.requested + ' → ' + (s.active || 'unavailable')).join(' | ');
        text += ' | live ' + sum('live') + ' | contacts ' + sum('contacts') + ' | query failures ' + sum('queryFailures');
        text += '\nPersistent game coating: ' + this.state.count + ' impacts / ' + this.state.weight.toFixed(2) +
            ' weight / ' + Object.keys(this.state.cells).length + ' receiver cells';
        text += ' | ' + (this.state.paused ? 'PARTICLES PAUSED' : 'particles playing');
        if (this.savePending) text += ' | finishing captured contacts before save';
        const diagnosis = stats.map(s => s.diagnostic).filter(Boolean).join(' | ');
        if (diagnosis) text += '\n' + diagnosis;
        if (this.request) {
            const operation = saves.status(this.request);
            text += '\nSave/load: ' + (operation?.state || 'pending') + ' ' + (operation?.error || '');
        }
        this.doc.get('status').text = text;
    }
    presentationUpdate() {
        let eye = this.state.eye;
        let forward = v(-Math.sin(this.state.yaw) * Math.cos(this.state.pitch),
            Math.sin(this.state.pitch), -Math.cos(this.state.yaw) * Math.cos(this.state.pitch));
        if (this.state.tour) {
            const angle = this.state.time * .035;
            const radius = this.kind === 'stress' ? 24 : 18;
            eye = v(radius * Math.sin(angle), this.kind === 'stress' ? 16 : 11,
                radius * Math.cos(angle));
            forward = norm(sub(v(0, 1, -3), eye));
            this.state.eye = eye;
            this.state.yaw = Math.atan2(-forward.x, -forward.z);
            this.state.pitch = Math.asin(forward.y);
        }
        this.view.setView({position: eye, rotation: look(forward, v(0, 1, 0))},
            65, {near: .1, far: 150});
    }
    destroy() {
        if (this.view?.valid) this.view.leave();
        input.pointerCapture = false;
    }
}

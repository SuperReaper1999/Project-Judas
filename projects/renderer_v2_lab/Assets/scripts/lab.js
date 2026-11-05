import {world, input, ui, scenes, saves, players} from 'judas';
import {add, mul, norm, cross, look, sub} from './math.js';

export const properties = {caseKey: {type: 'string', default: 'ordinary'}};

const vector = (x = 0, y = 0, z = 0) => ({x, y, z});
const CASES = [
    {key: 'ordinary', title: 'Ordinary gallery', count: '256 repeated modules / 16 authored lights',
        detail: 'A modest starting view. Add shadows, live monitors or split views separately.', radius: 53},
    {key: 'mixed', title: 'Mixed PBR materials', count: '1,024 modules / 64 normal-mapped PBR materials / 32 lights',
        detail: 'Different roughness, metalness and material images split compatible draw groups.', radius: 55},
    {key: 'sprites', title: 'Ordered sprite field', count: '4,096 authored sprites / one shared image',
        detail: 'Overlapping alpha, sort layers, Y sorting, flipped pivots and nearest/linear sampling.', flat: true},
    {key: 'particles', title: 'Generic particle presentation', count: '8 generic emitters / up to 10,000 live visual particles',
        detail: 'Ordinary CPU particle simulation and camera-sorted presentation. This is not the weather effect.', radius: 39},
    {key: 'lights', title: 'Overlapping lights and shadows', count: '1,024 modules / 256 overlapping authored lights',
        detail: 'Deliberate light-list stress. Lower-relevance overflow remains visible in the profiler.', radius: 54},
    {key: 'repeated', title: 'Repeated-object stress', count: '4,096 compatible repeated modules / 16 lights',
        detail: 'A dense control for instancing and culling. It does not reduce the ordinary physics cadence.', radius: 64},
];

export default class RendererWorkshop {
    constructor({properties}) {
        this.case = CASES.find(value => value.key === properties.caseKey) || CASES[0];
        this.state = {
            time: 0, tour: true, lights: true, shadows: true, split: false, monitors: false,
            eye: vector(0, 18, 53), yaw: 0, pitch: -.28, viewIds: [],
        };
        this.views = [];
        this.request = 0;
        this.statusAge = 1;
        this.presentationFrames = 0;
        this.presentationRate = 0;
        this.rateStart = Date.now();
    }
    start() { this.bind(); }
    restore() { this.bind(); }
    bind() {
        this.doc = ui.get('rendering_lab');
        this.doc.modal = false;
        this.doc.get('pause').visible = false;
        ui.debugOverlayVisible = false;
        this.lamps = world.queryTags(['lamp']);
        this.cameras = world.queryTags(['gallery_camera']);
        this.views = (this.state.viewIds || []).map(id => players.get(id)).filter(value => value?.valid);
        this.setLights();
        this.setMonitors();
        this.setSplit(this.state.split);
        this.capturePointer();
        for (let index = 0; index < CASES.length; ++index) {
            const selected = this.case.key === CASES[index].key;
            this.doc.get('case_' + (index + 1)).text = (selected ? '> ' : '') + (index + 1) + ' ' +
                ['Ordinary', 'Materials', 'Sprites', 'Particles', 'Lights', 'Repeated'][index];
        }
        this.doc.get('title').text = 'JUDAS RENDERER V2 / ' + this.case.title.toUpperCase();
        this.statusAge = 1;
        this.rateStart = Date.now();
        this.presentationFrames = 0;
    }
    capturePointer() { input.pointerCapture = !this.doc.modal && !this.state.tour; }
    setLights() {
        for (const lamp of this.lamps) if (lamp.valid) {
            lamp.setLight({enabled: this.state.lights,
                castsShadow: this.state.shadows && lamp.light.kind === 'spot'});
        }
    }
    setMonitors() {
        for (const camera of this.cameras) if (camera.valid) camera.setCameraEnabled(this.state.monitors);
    }
    setSplit(enabled) {
        for (const player of this.views) if (player.valid) player.leave();
        this.views = [];
        this.state.viewIds = [];
        this.state.split = enabled;
        if (!enabled) return;
        this.views = [players.create('Workshop west'), players.create('Workshop east')];
        this.views[0].viewport = {x: 0, y: 0, width: .5, height: 1};
        this.views[1].viewport = {x: .5, y: 0, width: .5, height: 1};
        this.state.viewIds = this.views.map(value => value.id);
    }
    pause(open) {
        this.doc.modal = open;
        this.doc.get('pause').visible = open;
        this.capturePointer();
    }
    openCase(index) {
        if (index >= 0 && index < CASES.length && CASES[index].key !== this.case.key)
            scenes.load('Scenes/' + CASES[index].key + '.judas');
    }
    uiUpdate() {
        this.openedPause = false;
        if (input.pressed('pause') && !this.doc.modal) {
            this.pause(true);
            this.openedPause = true;
        }
    }
    onUI(event) {
        if (event.document !== 'rendering_lab') return;
        if ((event.type === 'back' && !this.openedPause) ||
            (event.type === 'click' && event.element === 'resume')) this.pause(false);
        if (event.type !== 'click') return;
        if (event.element === 'reload') scenes.reload();
        if (event.element.startsWith('case_')) this.openCase(Number(event.element.slice(5)) - 1);
    }
    update(dt) {
        if (this.doc.modal) return;
        this.state.time += dt;
        this.statusAge += dt;
        if (input.pressed('watch')) { this.state.tour = !this.state.tour; this.capturePointer(); }
        if (input.pressed('lights')) { this.state.lights = !this.state.lights; this.setLights(); }
        if (input.pressed('shadows')) { this.state.shadows = !this.state.shadows; this.setLights(); }
        if (input.pressed('split')) this.setSplit(!this.state.split);
        if (input.pressed('monitors')) { this.state.monitors = !this.state.monitors; this.setMonitors(); }
        for (let index = 0; index < CASES.length; ++index)
            if (input.pressed('case_' + (index + 1))) { this.openCase(index); return; }
        if (input.pressed('reset')) scenes.reload();
        if (input.pressed('save')) this.request = saves.save('renderer-v2-workshop', {name: this.case.title});
        if (input.pressed('load')) this.request = saves.load('renderer-v2-workshop');
        if (!this.state.tour) {
            if (this.case.flat) {
                this.state.eye.x += input.axis('move_x') * dt * 20;
                this.state.eye.y += input.axis('move_z') * dt * 20;
            } else {
                this.state.yaw -= input.axis('look_x') * .002;
                this.state.pitch = Math.max(-1.4, Math.min(1.4,
                    this.state.pitch - input.axis('look_y') * .002));
                const forward = vector(-Math.sin(this.state.yaw), 0, -Math.cos(this.state.yaw));
                const right = cross(forward, vector(0, 1, 0));
                const direction = add(add(mul(forward, input.axis('move_z')),
                    mul(right, input.axis('move_x'))), vector(0, input.axis('move_y'), 0));
                this.state.eye = add(this.state.eye, mul(norm(direction), dt * 22));
            }
        }
        if (this.statusAge < .3) return;
        this.statusAge = 0;
        let text = this.case.count + '\n' + this.case.detail + '\n';
        text += this.state.tour ? 'WATCH CAMERA — T takes control' : 'FREE CAMERA — T returns to watch';
        text += ' | ' + (this.state.split ? '2 PLAYER VIEWS' : '1 MAIN VIEW');
        text += ' | monitors ' + (this.state.monitors && this.cameras.length ? 'ON' : 'OFF');
        text += '\nLights ' + (this.state.lights ? 'ON' : 'OFF') + ' | spot requests ' +
            (this.state.shadows ? 'ON' : 'OFF') + ' | presentation ~' + this.presentationRate.toFixed(1) + ' Hz';
        text += ' | Editor F8: CPU / GPU / draw diagnostics';
        if (this.request) {
            const operation = saves.status(this.request);
            text += '\nSave/load: ' + (operation?.state || 'pending') + ' ' + (operation?.error || '');
        }
        this.doc.get('status').text = text;
    }
    presentationUpdate() {
        ++this.presentationFrames;
        const now = Date.now(), elapsed = now - this.rateStart;
        if (elapsed >= 1000) {
            this.presentationRate = this.presentationFrames * 1000 / elapsed;
            this.presentationFrames = 0;
            this.rateStart = now;
        }
        let eye, rotation, range;
        if (this.case.flat) {
            eye = this.state.tour ? vector(2 * Math.sin(this.state.time * .1), 0, 25) :
                vector(this.state.eye.x, this.state.eye.y, 25);
            if (this.state.tour) this.state.eye = eye;
            rotation = {w: 1, x: 0, y: 0, z: 0};
            range = {near: .1, far: 100, projection: 'orthographic', height: 52};
        } else {
            eye = this.state.eye;
            let forward = vector(-Math.sin(this.state.yaw) * Math.cos(this.state.pitch),
                Math.sin(this.state.pitch), -Math.cos(this.state.yaw) * Math.cos(this.state.pitch));
            if (this.state.tour) {
                const angle = this.state.time * .07;
                eye = vector(this.case.radius * Math.sin(angle), 20 + 4 * Math.sin(angle * .7),
                    this.case.radius * Math.cos(angle));
                forward = norm(sub(vector(0, 1, 0), eye));
                // Handing over preserves the watch position instead of teleporting.
                this.state.eye = eye;
                this.state.yaw = Math.atan2(-forward.x, -forward.z);
                this.state.pitch = Math.asin(forward.y);
            }
            rotation = look(forward, vector(0, 1, 0));
            range = {near: .1, far: 250};
        }
        world.setView({position: eye, rotation}, 65, range);
        for (let index = 0; index < this.views.length; ++index) {
            const player = this.views[index];
            if (!player.valid) continue;
            const position = this.case.flat ? add(eye, vector(index * 5 - 2.5, 0, 0)) :
                vector(index ? -eye.x : eye.x, 20, index ? -eye.z : eye.z);
            const orientation = this.case.flat ? rotation : look(norm(sub(vector(0, 1, 0), position)), vector(0, 1, 0));
            player.setView({position, rotation: orientation}, 65, range);
        }
    }
    destroy() {
        for (const player of this.views) if (player.valid) player.leave();
        input.pointerCapture = false;
        world.clearView();
    }
}

import { world, physics, input, saves, ui, console } from 'judas';
import { WORLD } from './world-data.js';
import { source } from './soil-source.js';
import Camera from './camera.js';
import Harvest from './harvest.js';
import GravityGun from './gravity-gun.js';
import Builder from './build.js';
import HUD from './hud.js';
import { move } from './movement.js';
import { recipes } from './inventory.js';
import { add, sub, mul, dot, length, norm, rotate, inverse, qm, axis } from './math.js';
export default class Player {
    constructor({ entity }) { this.entity = entity; this.state = { schema: 1, inventory: { wood: 0, stone: 0 }, owned: { axe: false, pickaxe: false, shovel: false }, equipped: 'gravity', yaw: 0, pitch: 0, third: false, selectedBuild: 'floor', buildRotation: 0, harvest: {}, built: [], heldRef: null, stats: { treesFelled: 0, treesProcessed: 0, woodCollected: 0, stoneMined: 0, woodSpent: 0, stoneSpent: 0, dug: 0, placed: 0, launched: 0, grabs: 0 }, lastAction: '', actionCount: 0 }; this.cooldown = 0; this.swing = 0; this.blocked = true; this.notice = 'Welcome home. Your tools are waiting inside the cottage.'; }
    start() { this.bind(false); }
    restore() { this.bind(true); }
    bind(restored) {
        this.camera = new Camera(this);
        this.grab = new GravityGun(this);
        this.harvest = new Harvest(this);
        this.builder = new Builder(this);
        this.state.heldRef = null;
        this.entity.character.ignore([]);
        this.hud = new HUD(this);
        if (restored) {
            this.hud.close();
            this.say('Welcome back. Your homestead has been restored.');
        }
        this.request = null;
        this.quitAfterSave = false;
        this.refreshToken = saves.refresh();
        this.avatar = world.entity(WORLD.avatar);
        this.clip = '';
        this.camera.publish();
        console.log('Hearthworld: WASD move, mouse look, Space jump, Shift sprint; E collect; 1 gravity / 2 axe / 3 pickaxe / 4 shovel / 5 build; LMB use, RMB grab/drop or dismantle, Q recipe, R rotate; Tab pack, Esc pause; F5 save / F9 load.');
    }
    say(text) { this.notice = text; this.state.lastAction = text; this.state.actionCount++; }
    feedback(text, sound, point) {
        this.say(text);
        this.swing = .28;
        const id = WORLD.sounds?.[sound], emitter = id ? world.entity(id) : null;
        if (emitter?.valid && emitter.audio?.ready) {
            emitter.transform = { position: point || this.entity.transform.position };
            try {
                emitter.playAudioOneShot();
            }
            catch (error) {
                if (!this.audioWarning) {
                    console.log('Hearthworld effect skipped: ' + error.message);
                    this.audioWarning = true;
                }
            }
        }
        const dust = world.entity(WORLD.dust);
        if (dust?.valid && point && ['chop', 'stone', 'dig', 'build'].includes(sound)) {
            dust.transform = { position: point, rotation: this.entity.transform.rotation };
            dust.burst(10);
        }
    }
    update(dt) { if (this.hud.modal)
        return; this.state.yaw -= input.axis('look_x') * .0021 + input.axis('look_stick_x') * 2.2 * dt; this.state.pitch = Math.max(-1.4, Math.min(1.4, this.state.pitch - input.axis('look_y') * .0021 - input.axis('look_stick_y') * 2.2 * dt)); if (input.pressed('camera_toggle'))
        this.state.third = !this.state.third; }
    fixedUpdate(dt) {
        if (this.hud.modal)
            return;
        this.cooldown = Math.max(0, this.cooldown - dt);
        this.swing = Math.max(0, this.swing - dt);
        const soil = world.entity(WORLD.terrain), terrain = soil.terrain;
        if (!terrain.state)
            terrain.create({ source, navigation: false });
        this.pollDig();
        if (terrain.state?.error && terrain.state.error !== this.terrainError) {
            this.terrainError = terrain.state.error;
            this.say('Terrain preparation failed; old geometry retained: ' + this.terrainError);
        }
        for (const kind of ['gravity', 'axe', 'pickaxe', 'shovel', 'build'])
            if (input.pressed('equip_' + kind)) {
                if (['gravity', 'build'].includes(kind) || this.state.owned[kind]) {
                    if (this.state.equipped !== kind)
                        this.grab.drop();
                    this.state.equipped = kind;
                    this.say(kind === 'build' ? 'Q changes recipe. Aim at ground or a floor; LMB places, R turns, RMB dismantles.' : kind === 'gravity' ? 'RMB grab / drop a loose object. LMB launches it.' : 'Equipped ' + kind + '. Aim and left click.');
                }
                else
                    this.say('Find the ' + kind + ' inside your cottage.');
            }
        move(this, dt);
        this.grab.tick(dt);
        this.builder.tick();
        if (!input.held('primary') && !input.held('grab'))
            this.blocked = false;
        if (input.pressed('interact'))
            this.harvest.interact(this.camera.hit(3.5));
        if (!this.blocked && this.cooldown <= 0 && (input.pressed('primary') || (this.state.equipped === 'shovel' && input.held('primary')))) {
            const hit = this.camera.hit(5);
            this.cooldown = .45;
            this.swing = .28;
            if (this.state.equipped === 'gravity') {
                if (this.grab.held)
                    this.grab.launch();
                else
                    this.say('RMB grabs a loose object first; LMB launches it.');
            }
            else if (this.state.equipped === 'build')
                this.builder.place(hit);
            else if (this.state.equipped === 'shovel')
                this.dig(hit);
            else
                this.harvest.use(hit);
        }
        this.animate(dt);
    }
    dig(hit) {
        if (!hit || hit.entity?.id !== WORLD.terrain) {
            this.say('Dig the marked backyard, behind the cottage. The rest of the planet is solid.');
            return;
        }
        const e = hit.entity, t = e.transform, local = rotate(inverse(t.rotation), sub(hit.point, t.position)), radius = 1.7;
        if (Math.hypot(local.x, local.z) + radius > WORLD.digRadius || local.y < 1.9) {
            this.say('Keep digging inside the garden boundary, above its solid foundation.');
            return;
        }
        const state = e.terrain.state;
        if (!state?.ready || state.busy) {
            this.say('Soil is updating; the last complete collision remains active.');
            return;
        }
        const center = sub(hit.point, mul(hit.normal, .55));
        if (e.terrain.brush(center, { kind: 'remove', radius, strength: 1 })) {
            this.pendingDig = { revision: state.revision, point: hit.point };
            this.feedback('Preparing excavation; the previous collision remains active.', 'dig', hit.point);
        }
        else
            this.say('Terrain preparation is busy; try again shortly.');
    }
    pollDig() { if (!this.pendingDig)
        return; const s = world.entity(WORLD.terrain).terrain.state; if (!s || s.busy)
        return; if (s.revision > this.pendingDig.revision) {
        this.state.stats.dug++;
        this.say('Excavation complete — the new soil and collision are live.');
        this.pendingDig = null;
    }
    else if (s.error) {
        this.say('Excavation failed; previous terrain retained: ' + s.error);
        this.pendingDig = null;
    } }
    context() {
        const s = this.state;
        if (this.grab.held?.valid)
            return 'HOLDING  ·  LMB launch  /  RMB release  /  R turn';
        const hit = this.camera.hit(4.5);
        if (!hit)
            return 'Explore your little world. Home is the teal-roof cottage.';
        const e = hit.entity;
        if (!e?.valid)
            return '';
        const tool = WORLD.tools.find(t => t.id === e.id);
        if (tool)
            return 'E  COLLECT ' + tool.kind.toUpperCase();
        if (e.hasTag('wood'))
            return 'E  COLLECT LOG  +3 WOOD  /  gravity gun to carry';
        if (s.equipped === 'build') {
            const r = recipes[s.selectedBuild];
            return 'BUILD ' + s.selectedBuild.toUpperCase() + '   ' + r.wood + ' wood / ' + r.stone + ' stone   ·  Q recipe / R turn';
        }
        if (e.hasTag('tree'))
            return this.state.harvest[e.id]?.felled ? 'AXE  ·  Chop the fallen trunk to make logs' : 'AXE  ·  Three chops to fell this physical tree';
        if (e.hasTag('rock'))
            return 'PICKAXE  ·  Mine stone';
        if (e.id === WORLD.terrain)
            return 'SHOVEL  ·  Dig your backyard pool';
        if (e.hasTag('carryable'))
            return 'GRAVITY GUN  ·  RMB grab';
        return '';
    }
    animate(dt) { if (!this.avatar?.valid)
        return; const m = this.entity.character, speed = length(sub(m.velocity, m.state.supportVelocity)), clip = !m.supported ? 'Jump' : speed > 4.8 ? 'Run' : speed > .35 ? 'Walk' : 'Idle'; const a = this.avatar.animation; if (a?.info.ready && clip !== this.clip) {
        a.crossFade(clip, .18);
        a.loop = true;
        this.clip = clip;
    } }
    presentationUpdate() {
        if (!this.camera)
            return;
        const view = this.camera.publish(), t = this.entity.presentedTransform;
        if (this.avatar?.valid) {
            this.avatar.renderVisible = this.state.third;
            this.avatar.transform = { position: t.position, rotation: qm(t.rotation, axis({ x: 0, y: 1, z: 0 }, this.state.yaw + Math.PI)) };
        }
        const selected = { gravity: 'gravity_gun', axe: 'axe', pickaxe: 'pickaxe', shovel: 'shovel', build: 'hammer' }[this.state.equipped];
        for (const [kind, id] of Object.entries(WORLD.viewmodels)) {
            const e = world.entity(id);
            if (!e?.valid)
                continue;
            e.renderVisible = !this.state.third && !this.hud.modal && kind === selected && !this.grab.held;
            if (e.renderVisible) {
                const swing = Math.sin((.28 - this.swing) / .28 * Math.PI) * (this.swing > 0 ? 1 : 0);
                e.transform = { position: add(view.position, rotate(view.rotation, { x: .4, y: -.55 - swing * .1, z: -.75 })), rotation: qm(qm(view.rotation, kind === 'gravity_gun' ? axis({ x: 0, y: 1, z: 0 }, Math.PI) : { w: 1, x: 0, y: 0, z: 0 }), axis({ x: 1, y: 0, z: 0 }, -.2 - swing * .9)), scale: { x: .42, y: .42, z: .42 } };
            }
        }
        this.builder.present();
    }
    requestSave() { this.pollDig(); if (this.pendingDig) {
        this.pendingSave = true;
        this.say('Waiting for the excavation to publish before saving…');
        return;
    } if (this.request) {
        this.say('Wait for the current save operation.');
        return;
    } try {
        this.grab.drop();
        this.request = saves.save('home', { name: 'My Hearthworld', metadata: { trees: this.state.stats.treesFelled, pieces: this.state.built.length } });
        this.saveKind = 'save';
        this.say('Saving your home…');
    }
    catch (error) {
        this.quitAfterSave = false;
        this.say('Save not started: ' + error.message);
    } }
    requestLoad() { if (this.request)
        return; try {
        this.request = saves.load('home');
        this.saveKind = 'load';
        this.say('Restoring your home…');
    }
    catch (error) {
        this.say('Load not started: ' + error.message);
    } }
    pollSave() {
        this.pollDig();
        if (this.pendingSave && !this.pendingDig) {
            this.pendingSave = false;
            this.requestSave();
        }
        if (this.refreshToken) {
            const r = saves.status(this.refreshToken);
            if (r && ['completed', 'failed', 'cancelled'].includes(r.state))
                this.refreshToken = null;
        }
        if (!this.request)
            return;
        const result = saves.status(this.request);
        if (!result)
            return;
        if (result.state === 'failed' || result.state === 'cancelled') {
            this.say('Save operation ' + result.state + ': ' + (result.error || ''));
            this.request = null;
            this.quitAfterSave = false;
            return;
        }
        if (result.state === 'completed') {
            this.say(this.saveKind === 'save' ? 'HOME SAVED. You can safely quit.' : 'Home restored.');
            this.request = null;
            if (this.quitAfterSave)
                ui.quit();
        }
    }
    uiUpdate() { this.hud?.update(); }
    onUI(e) { this.hud?.event(e); }
    destroy() { this.grab?.drop(); this.hud?.destroy(); input.pointerCapture = false; world.clearView(); }
}

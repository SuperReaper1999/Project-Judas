import { world, input, saves, ui, console } from 'judas';
import { ITEMS } from './definitions.js';
import { WORLD } from './world-data.js';
import * as Bag from './inventory.js';
import { initialVitals, consume } from './survival.js';
import { move } from './movement.js';
import { collect, useTool, nodeFor, harvestStatus } from './harvesting.js';
import { add, mul, sub, length, dot, rotate, qm, axis } from './math.js';
import Camera from './camera.js';
import Transport from './transport.js';
import { explore } from './exploration.js';
import HUD from './hud.js';
import Housing from './housing.js';
import Construction from './construction.js';
import Combat from './combat.js';
import { PEOPLE, WEAPONS } from './encounter-data.js';
import { actorFor } from './people-session.js';
import { RECIPES } from './definitions.js';
export default class Player {
    constructor({ entity }) {
        this.entity = entity;
        let bag = Bag.createBag();
        for (const [id, n] of Object.entries({
            work_coat: 1,
            empty_flask: 1,
            rations: 2,
            tokens: 20,
        }))
            bag = Bag.add(bag, id, n).bag;
        this.state = {
            schema: 1,
            bag,
            equipment: { body: 'work_coat' },
            equipped: 'hands',
            hotbar: [
                'hands',
                'stone_axe',
                'stone_pickaxe',
                'berries',
                'water',
                'bandage',
            ],
            vitals: initialVitals(),
            yaw: 0,
            pitch: -0.05,
            third: false,
            sensitivity: 0.0021,
            harvest: {},
            drops: {},
            introSeen: false,
            stats: {
                gathered: 0,
                crafted: 0,
                felled: 0,
                timber: 0,
                mined: 0,
                cooked: 0,
            },
        };
        this.cooldown = 0;
        this.swing = 0;
        this.blocked = true;
        this.notice = 'Welcome to the homestead coast.';
    }
    start() {
        this.bind(false);
    }
    restore() {
        this.bind(true);
    }
    bind(restored) {
        this.state.exploration ||= { visited: {}, target: null };
        this.transport = new Transport(this);
        this.camera = new Camera(this);
        this.housing = new Housing(this);
        this.combat = new Combat(this);
        this.hud = new HUD(this, restored);
        this.builder = new Construction(this);
        this.request = null;
        this.quitAfterSave = false;
        this.refreshToken = saves.refresh();
        this.camera.publish();
        this.clip = '';
        console.log(
            'Lunar Island: WASD, mouse, E gather, click tool, Tab pack/crafting, Escape pause, F6 save, F9 load.',
        );
    }
    say(text) {
        this.notice = text;
    }
    effect(sound, text) {
        this.say(text);
        this.swing = 0.3;
        const e = world.entity(WORLD.sounds[sound] || PEOPLE.sounds[sound]);
        if (e?.valid && e.audio?.ready) {
            e.transform = { position: this.entity.transform.position };
            try {
                e.playAudioOneShot();
            } catch (error) {
                if (!this.audioWarning) {
                    this.audioWarning = String(error);
                    console.log(
                        'Optional audio feedback skipped: ' + error.message,
                    );
                }
            }
        }
    }
    atFire() {
        return (
            length(
                sub(this.entity.transform.position, {
                    x: WORLD.campfire[0],
                    y: WORLD.campfire[1],
                    z: WORLD.campfire[2],
                }),
            ) < 4
        );
    }
    craft(id) {
        const result = Bag.craft(
            this.state.bag,
            id,
            RECIPES.find((r) => r.id === id)?.station === 'workbench'
                ? this.housing.atStation('workbench')
                    ? 'workbench'
                    : 'hand'
                : this.atFire()
                  ? 'campfire'
                  : 'hand',
        );
        if (!result.ok) {
            this.say(result.reason);
            return false;
        }
        this.state.bag = result.bag;
        this.state.stats.crafted++;
        if (id === 'cooked_meal') this.state.stats.cooked++;
        if (ITEMS[id]?.tool) this.state.equipped = id;
        this.effect('craft', 'Crafted ' + (ITEMS[id]?.name || id) + '.');
        return true;
    }
    reconcileEquipment() {
        // Magazines are per carried weapon type in this stage. Automatically
        // unload the last weapon when it leaves the pack, retaining its rounds.
        if (this.state.combat)
            for (const [id, rounds] of Object.entries(
                this.state.combat.magazines,
            )) {
                if (rounds > 0 && !Bag.count(this.state.bag, id)) {
                    const r = Bag.add(this.state.bag, WEAPONS[id].ammo, rounds);
                    if (r.ok) {
                        this.state.bag = r.bag;
                        this.state.combat.magazines[id] = 0;
                    }
                }
            }
        for (const key of Object.keys(this.state.equipment))
            if (!Bag.count(this.state.bag, this.state.equipment[key]))
                delete this.state.equipment[key];
        if (
            this.state.equipped !== 'hands' &&
            !Bag.count(this.state.bag, this.state.equipped)
        )
            this.state.equipped = 'hands';
    }
    useSlot(index) {
        const slot = this.state.bag.slots[index];
        if (!slot) return;
        const def = ITEMS[slot.item];
        if (def.slot) {
            this.state.equipment[def.slot] = slot.item;
            if (def.slot === 'hand') this.state.equipped = slot.item;
            this.say('Equipped ' + def.name + '.');
            return;
        }
        if (!def.use) {
            this.say(
                def.category === 'supplies'
                    ? 'Carry this supply, then E on a vehicle → Refuel or Repair.'
                    : 'Use this material in Crafting.',
            );
            return;
        }
        const result = Bag.exchange(
            this.state.bag,
            { [slot.item]: 1 },
            def.returns ? { [def.returns]: 1 } : {},
        );
        if (!result.ok) {
            this.say(result.reason);
            return;
        }
        this.state.bag = result.bag;
        this.state.vitals = consume(this.state.vitals, def.use);
        this.reconcileEquipment();
        this.effect('consume', 'Used ' + def.name + '.');
    }
    dropSlot(index) {
        const slot = this.state.bag.slots[index];
        if (!slot) return;
        const debit = Bag.remove(this.state.bag, slot.item, 1);
        const view = this.camera.pose(),
            position = add(view.eye, mul(view.direction, 1.3));
        let e;
        try {
            e = world.spawnPrefab(WORLD.lootPrefab, { position });
            e.velocity = add(
                this.entity.character.velocity,
                mul(view.direction, 1.2),
            );
        } catch (error) {
            this.say('Could not drop item: ' + error.message);
            return;
        }
        this.state.bag = debit.bag;
        this.reconcileEquipment();
        this.state.drops[e.id] = { item: slot.item, count: 1 };
        if (!Bag.count(this.state.bag, slot.item)) {
            for (const key of Object.keys(this.state.equipment))
                if (this.state.equipment[key] === slot.item)
                    delete this.state.equipment[key];
            if (this.state.equipped === slot.item)
                this.state.equipped = 'hands';
        }
        this.say(
            'Dropped ' +
                ITEMS[slot.item].name +
                '. E recovers the physical bundle.',
        );
    }
    interact(hit) {
        if (this.transport.interact(hit)) return;
        if (this.combat.interact(hit)) return;
        if (this.housing.interact(hit)) return;
        if (hit?.entity?.id === WORLD.well) {
            const has = Bag.count(this.state.bag, 'empty_flask');
            if (has) {
                const r = Bag.exchange(
                    this.state.bag,
                    { empty_flask: 1 },
                    { water: 1 },
                );
                if (r.ok) {
                    this.state.bag = r.bag;
                    this.effect(
                        'consume',
                        'Flask refilled with fresh spring water.',
                    );
                } else this.say(r.reason);
            } else {
                this.state.vitals.thirst = 100;
                this.effect('consume', 'Drank fresh spring water.');
            }
            return;
        }
        if (!collect(this, hit))
            this.say(
                'E gathers loose sticks, stones and plants. Use tools on trunks and deposits.',
            );
    }
    update(dt) {
        if (this.hud.modal) return;
        this.state.yaw -=
            input.axis('look_x') * this.state.sensitivity +
            input.axis('look_stick_x') * 2.2 * dt;
        this.state.pitch = Math.max(
            -1.4,
            Math.min(
                1.4,
                this.state.pitch -
                    input.axis('look_y') * this.state.sensitivity -
                    input.axis('look_stick_y') * 2.2 * dt,
            ),
        );
        if (input.pressed('camera_toggle'))
            this.state.third = !this.state.third;
    }
    fixedUpdate(dt) {
        if (this.hud.page === 'loading') {
            this.entity.character.velocity = { x: 0, y: 0, z: 0 };
            return;
        }
        if (this.hud.modal) return;
        this.cooldown = Math.max(0, this.cooldown - dt);
        this.swing = Math.max(0, this.swing - dt);
        explore(this);
        if (this.transport.fixedUpdate(dt)) {
            if (this.state.vitals.health <= 0) this.hud.open('dead');
            return;
        }
        this.combat.tick(dt);
        move(this, dt);
        if (this.state.vitals.health <= 0) {
            this.hud.open('dead');
            return;
        }
        for (let i = 1; i <= 6; i++)
            if (input.pressed('hotbar_' + i)) {
                const id = this.state.hotbar[i - 1];
                if (id === 'hands' || Bag.count(this.state.bag, id)) {
                    this.state.equipped = id;
                    this.say(
                        id === 'hands'
                            ? 'Hands ready.'
                            : ITEMS[id].name + ' ready.',
                    );
                } else this.say('You do not have ' + ITEMS[id].name + ' yet.');
            }
        if (this.blocked) {
            if (!input.held('primary') && !input.held('interact'))
                this.blocked = false;
            return;
        }
        this.builder.tick();
        const hit = this.camera.hit(3.8);
        if (this.builder.active) {
            if (input.pressed('primary')) this.builder.confirm();
            return;
        }
        if (input.pressed('interact')) this.interact(hit);
        if (
            (input.pressed('primary') ||
                (this.combat.weapon?.automatic && input.held('primary'))) &&
            this.cooldown === 0
        ) {
            this.cooldown = 0.45;
            this.swing = 0.3;
            if (ITEMS[this.state.equipped]?.use) {
                const slot = this.state.bag.slots.findIndex(
                    (s) => s?.item === this.state.equipped,
                );
                this.useSlot(slot);
            } else if (
                this.combat.weapon?.ammo ||
                ['hands', 'knife', 'bat'].includes(this.state.equipped) ||
                actorFor(hit?.entity)
            )
                this.combat.fire();
            else if (!this.housing.damage(hit)) useTool(this, hit);
        }
    }
    context() {
        if (this.transport.driving)
            return 'E · Leave when stopped / Tab · Cargo';
        const hit = this.camera.hit(3.8);
        const vehicleContext = this.transport.context(hit);
        if (vehicleContext) return vehicleContext;
        const personContext = this.combat.context(hit);
        if (personContext) return personContext;
        const housingContext = this.housing.context(hit);
        if (housingContext) return housingContext;
        if (hit?.entity?.id === WORLD.well)
            return 'E · Refill flask / drink fresh spring water';
        if (hit?.entity && this.state.drops[hit.entity.id])
            return (
                'E · Recover dropped ' +
                ITEMS[this.state.drops[hit.entity.id].item].name
            );
        const n = nodeFor(hit);
        if (!n)
            return this.atFire()
                ? 'Tab → Crafting · Cook beside the homestead fire'
                : '';
        if (n.kind === 'tree')
            return harvestStatus(this, n)?.felled
                ? 'AXE · Process fallen timber'
                : 'AXE · Fell this physical tree';
        return ['ore', 'rock'].includes(n.kind)
            ? 'PICKAXE · Mine ' + (n.kind === 'ore' ? 'iron ore' : 'stone')
            : 'E · Gather ' +
                  ({
                      branch: 'timber',
                      loose_stone: 'stone',
                      fiber: 'plant fibre',
                      berries: 'berries',
                  }[n.kind] || n.kind);
    }
    recover() {
        this.state.transport.driving = null;
        this.state.transport.throttle = 0;
        this.entity.character.enabled = true;
        this.entity.transform = {
            position: this.housing.homePosition() || {
                x: WORLD.spawn[0],
                y: WORLD.spawn[1],
                z: WORLD.spawn[2],
            },
        };
        this.entity.character.velocity = { x: 0, y: 0, z: 0 };
        this.state.vitals = initialVitals();
        this.fallSpeed = 0;
        this.say('Recovered at camp. Your pack is intact.');
    }
    presentationUpdate() {
        if (!this.camera) return;
        const view = this.camera.publish(),
            t = this.entity.presentedTransform;
        const a = world.entity(WORLD.avatar);
        if (a?.valid) {
            a.renderVisible = this.state.third && !this.transport.driving;
            a.transform = {
                position: t.position,
                rotation: qm(
                    t.rotation,
                    axis({ x: 0, y: 1, z: 0 }, this.state.yaw + Math.PI),
                ),
            };
            const speed = length(this.entity.character.velocity),
                clip =
                    !this.entity.character.supported &&
                    Math.abs(
                        dot(
                            this.entity.character.velocity,
                            this.entity.character.up,
                        ),
                    ) > 0.8
                        ? 'Jump'
                        : speed > 5
                          ? 'Run'
                          : speed > 0.4
                            ? 'Walk'
                            : 'Idle';
            // A contact edge can alternate support for several fixed steps. Finish
            // each short blend before requesting another; do not accumulate
            // interrupted pose contributors from noisy locomotion observations.
            const info = a.animation?.info;
            if (
                this.state.third &&
                info?.ready &&
                !info.transitioning &&
                clip !== this.clip
            ) {
                a.animation.crossFade(clip, 0.2);
                a.animation.loop = true;
                this.clip = clip;
            }
        }
        this.builder?.present();
        for (const [id, entity] of Object.entries({
            ...WORLD.viewmodels,
            ...PEOPLE.viewmodels,
        })) {
            const e = world.entity(entity);
            e.renderVisible =
                !this.transport.driving &&
                !this.state.third &&
                !this.hud.modal &&
                this.state.equipped === id;
            if (e.renderVisible)
                e.transform = {
                    position: add(
                        view.position,
                        rotate(view.rotation, { x: 0.42, y: -0.5, z: -0.7 }),
                    ),
                    rotation: qm(
                        view.rotation,
                        axis(
                            { x: 1, y: 0, z: 0 },
                            -0.25 -
                                Math.sin((this.swing / 0.3) * Math.PI) * 0.8,
                        ),
                    ),
                    scale: { x: 0.42, y: 0.42, z: 0.42 },
                };
        }
    }
    requestSave() {
        this.builder?.cancel();
        if (this.request) return;
        // Conversation is transient UI: never resume a spoken topic on cold load.
        this.hud?.voice.silence();
        try {
            this.request = saves.save('island', {
                name: 'Lunar Island / Homestead Coast',
                metadata: { gathered: this.state.stats.gathered, stage: 6 },
            });
            this.operation = 'save';
            this.say('Saving your expedition…');
        } catch (error) {
            this.quitAfterSave = false;
            this.say('Save not started: ' + error.message);
        }
    }
    requestLoad() {
        if (this.request) return;
        try {
            this.request = saves.load('island');
            this.operation = 'load';
            this.say('Restoring your expedition…');
        } catch (error) {
            this.say('Load not started: ' + error.message);
        }
    }
    pollSave() {
        if (this.refreshToken) {
            const r = saves.status(this.refreshToken);
            if (r && ['completed', 'failed', 'cancelled'].includes(r.state))
                this.refreshToken = null;
        }
        if (!this.request) return;
        const r = saves.status(this.request);
        if (!r) return;
        if (r.state === 'completed') {
            this.request = null;
            this.say(
                this.operation === 'save'
                    ? 'Expedition saved.'
                    : 'Expedition restored.',
            );
            if (this.quitAfterSave) ui.quit();
        } else if (['failed', 'cancelled'].includes(r.state)) {
            this.request = null;
            this.quitAfterSave = false;
            this.say('Save operation failed: ' + r.error);
        }
    }
    uiUpdate() {
        this.hud?.update();
    }
    onUI(e) {
        this.hud?.event(e);
    }
    destroy() {
        this.hud?.voice.stop();
        this.builder?.cancel();
        this.combat?.destroy();
        input.pointerCapture = false;
        world.clearView();
    }
}

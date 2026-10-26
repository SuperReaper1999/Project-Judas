import { ui, input, saves, navigation, world } from 'judas';
import { ITEMS, RECIPES, CONFIG } from './definitions.js';
import { WORLD } from './world-data.js';
import { renderVehicle, vehicleEvent } from './vehicle-ui.js';
import { VEHICLES } from './vehicle-data.js';
import { destinationRows, destinationText, guidance } from './exploration.js';
import { HOUSING } from './housing-data.js';
import { entityFor } from './housing.js';
import { count, weight } from './inventory.js';
import {
    socialPage,
    socialRows,
    renderSocial,
    socialEvent,
} from './social-ui.js';
export default class HUD {
    constructor(p, restored = false) {
        this.p = p;
        this.doc = ui.get('lunar');
        if (!this.doc) throw new Error('Lunar Island UI component is missing.');
        ui.debugOverlayVisible = false;
        this.page = '';
        this.selected = '';
        this.lastRows = '';
        if (restored || p.state.introSeen) this.close();
        else this.open('loading');
    }
    get modal() {
        return !!this.page;
    }
    open(page = 'pack') {
        this.p.builder?.cancel();
        this.page = page;
        this.selected = '';
        this.lastRows = '';
        this.doc.modal = page !== 'loading';
        input.pointerCapture = false;
        this.p.blocked = true;
        this.refresh();
    }
    close() {
        this.page = '';
        this.doc.modal = false;
        input.pointerCapture = true;
        this.p.blocked = true;
        this.doc.get('overlay').visible = false;
        this.p.state.introSeen = true;
    }
    inspect(ref) {
        this.objectRef = ref;
        this.open('object');
    }
    openStorage(ref, vehicle = false) {
        this.vehicleRef = vehicle ? ref : null;
        this.containerRef = ref;
        this.containerSelected = '';
        this.containerSignature = '';
        this.open('storage');
    }
    rows() {
        if (this.page === 'map') return destinationRows(this.p);
        if (socialPage(this.page)) return socialRows(this);
        const bag = this.p.state.bag;
        if (this.page === 'pack' || this.page === 'storage')
            return bag.slots.flatMap((s, i) =>
                s
                    ? [
                          {
                              id: String(i),
                              text: `${ITEMS[s.item].name}  ×${s.count}     ${Number(ITEMS[s.item].weight * s.count).toFixed(1)} kg`,
                          },
                      ]
                    : [],
            );
        if (this.page === 'craft')
            return RECIPES.map((r) => ({
                id: r.id,
                text: r.name + '  /  ' + r.station,
            }));
        if (this.page === 'build')
            return Object.entries(HOUSING.catalog).map(([id, d]) => ({
                id,
                text: d.category + ' / ' + d.name,
            }));
        return [];
    }
    refresh() {
        const d = this.doc,
            p = this.p,
            bag = p.state.bag,
            page = this.page;
        d.get('overlay').visible = !!page;
        if (!page) return;
        const list = [
            'pack',
            'craft',
            'build',
            'storage',
            'dialogue',
            'trade',
            'corpse',
            'map',
        ].includes(page);
        const storage = page === 'storage',
            object =
                page === 'object' || page === 'vehicle' || socialPage(page);
        d.get('entries2').visible = storage;
        d.get('secondary').visible = storage || object;
        d.get('tertiary').visible = storage || object;
        d.get('primary').visible = list || object;
        d.get('drop').visible = page === 'pack' || storage || object;
        d.get('primary').enabled = true;
        d.get('secondary').enabled = true;
        d.get('drop').enabled = true;
        d.get('tertiary').enabled = true;
        d.get('entries').setLayout({
            offset: { x: 24, y: storage ? 160 : 132 },
            size: { x: 520, y: storage ? 220 : 330 },
        });
        d.get('primary').setLayout({
            offset: { x: storage ? 24 : 568, y: 405 },
        });
        d.get('drop').setLayout({ offset: { x: storage ? 281 : 823, y: 405 } });
        d.get('secondary').setLayout({
            offset: { x: storage ? 568 : 568, y: storage ? 405 : 455 },
        });
        d.get('tertiary').setLayout({
            offset: { x: 823, y: storage ? 405 : 455 },
        });
        d.get('entries').visible = list;
        d.get('primary').visible = list || object;
        d.get('drop').visible = page === 'pack' || storage || object;
        d.get('map_image').visible = page === 'map' && !!this.referenceMap;
        if (page === 'map') {
            d.get('entries').visible = !this.referenceMap;
            d.get('secondary').visible = true;
            d.get('secondary').text = this.referenceMap
                ? 'Location list'
                : 'Reference map';
        }
        d.get('sensitivity').visible = page === 'help';
        if (page === 'help')
            d.get('sensitivity').value = Math.max(
                0,
                Math.min(1, (p.state.sensitivity - 0.0007) / 0.004),
            );
        if (['help', 'travel', 'welcome'].includes(page)) {
            d.get('secondary').visible = true;
            d.get('secondary').text =
                page === 'help' ? 'Vehicles & travel' : 'On-foot controls';
        }
        if (page === 'map') {
            d.get('tertiary').visible = !!p.state.exploration?.target;
            d.get('tertiary').text = 'Stop tracking';
        }
        d.get('title').text =
            {
                loading: 'PREPARING THE HOMESTEAD',
                pack: 'FIELD PACK',
                build: 'CONSTRUCTION & FURNISHING',
                storage: 'STORAGE / LOOT',
                object: 'WORLD OBJECT',
                craft: 'CRAFTING',
                map: 'LUNAR ISLAND / FIELD MAP',
                help: 'ON-FOOT CONTROLS',
                travel: 'VEHICLES & TRAVEL',
                pause: 'TAKE A BREATH',
                welcome: 'WELCOME TO LUNAR ISLAND',
                dead: 'RECOVER AT THE HOMESTEAD',
            }[page] || 'FIELD PACK';
        d.get('close').text =
            page === 'welcome'
                ? 'Begin your island'
                : page === 'dead'
                  ? 'Recover at camp'
                  : 'Return to island';
        d.get('menu_status').text =
            p.notice +
            `\nPack ${weight(bag).toFixed(1)} / ${bag.maxWeight} kg · ${bag.slots.filter(Boolean).length}/${bag.slots.length} slots`;
        const rows = this.rows(),
            signature = JSON.stringify(rows);
        if (signature !== this.lastRows) {
            d.get('entries').setItems(rows);
            this.lastRows = signature;
            if (rows.length && !rows.some((r) => r.id === this.selected))
                this.selected = rows[0].id;
        }
        let text = '';
        if (page === 'vehicle') text = renderVehicle(this);
        else if (socialPage(page)) text = renderSocial(this);
        else if (page === 'storage') {
            const bag2 = this.vehicleRef
                ? p.transport.cargo(this.vehicleRef)
                : p.housing.state.containers[this.containerRef];
            const rows2 = bag2
                ? bag2.slots.flatMap((slot, i) =>
                      slot
                          ? [
                                {
                                    id: String(i),
                                    text:
                                        ITEMS[slot.item].name +
                                        ' ×' +
                                        slot.count,
                                },
                            ]
                          : [],
                  )
                : [];
            const signature2 = JSON.stringify(rows2);
            if (signature2 !== this.containerSignature) {
                d.get('entries2').setItems(rows2);
                this.containerSignature = signature2;
                if (!rows2.some((x) => x.id === this.containerSelected))
                    this.containerSelected = rows2[0]?.id || '';
            }
            if (this.vehicleRef)
                d.get('title').text =
                    'CARGO / ' + p.transport.cargoName(this.vehicleRef);
            text = bag2
                ? 'BACKPACK ↔ STORAGE   /   ' +
                  weight(bag2).toFixed(1) +
                  ' / ' +
                  bag2.maxWeight +
                  ' kg in storage'
                : 'Storage unavailable';
            d.get('primary').text = 'Store one';
            d.get('drop').text = 'Store stack';
            d.get('secondary').text = 'Take one';
            d.get('tertiary').text = 'Take stack';
        } else if (page === 'build') {
            const def = HOUSING.catalog[this.selected];
            text = def
                ? def.name +
                  '\n\n' +
                  Object.entries(def.cost)
                      .map(
                          ([id, n]) =>
                              ITEMS[id].name +
                              '  ' +
                              count(bag, id) +
                              ' / ' +
                              n,
                      )
                      .join('\n') +
                  '\n\nA physical piece connects to the support you aim at. Clear space is required. R rotates; click confirms. Paintings need a wall.'
                : 'Choose a construction or furniture piece.';
            d.get('primary').text = 'Start placement';
        } else if (page === 'object') {
            const e = entityFor(this.objectRef),
                r = e?.valid ? p.housing.record(e) : null,
                socket = e?.valid ? p.housing.socket(e) : null,
                def = r && HOUSING.catalog[r.kind];
            text = r
                ? (def?.name || 'Barricade') +
                  '\n\nIntegrity ' +
                  r.health +
                  ' / ' +
                  r.maxHealth +
                  '\n\n' +
                  (p.housing.state.containers[this.objectRef]
                      ? 'Contents remain with this object when moved. Empty it before dismantling.'
                      : r.kind === 'bed'
                        ? 'Rest costs 5 food/water, restores stamina and some health, and claims this bed as your recovery point.'
                        : r.kind === 'workbench'
                          ? 'Craft nails and sheets within 3 metres.'
                          : r.barricade
                            ? 'Axe/pick impacts damage this real collision barrier. Repair or reinforce it before it breaks.'
                            : 'Move/rotate through the placement preview. Escape cancels without changing the original object.')
                : socket
                  ? 'SECURE AN OPENING\n\nWood: 2 planks + 1 nail\nMetal: 1 sheet + 2 nails\n\nClose a door before barricading. The barrier has real collision and a fixed physical connection.'
                  : 'Object no longer available.';
            d.get('primary').text = r?.barricade
                ? 'Repair'
                : r && p.housing.state.containers[this.objectRef]
                  ? 'Open storage'
                  : r?.kind === 'bed'
                    ? 'Rest / claim home'
                    : r?.kind === 'lamp'
                      ? 'Toggle lamp'
                      : r?.kind === 'workbench'
                        ? 'Workbench crafting'
                        : socket
                          ? 'Wood barricade'
                          : 'Inspect';
            d.get('primary').enabled = !!(
                socket ||
                r?.barricade ||
                def?.use ||
                p.housing.state.containers[this.objectRef]
            );
            d.get('drop').text = 'Dismantle';
            d.get('drop').enabled = !!r;
            d.get('secondary').text = r?.barricade
                ? 'Reinforce with metal'
                : r
                  ? 'Move / rotate'
                  : 'Metal barricade';
            d.get('secondary').enabled = r?.barricade
                ? !r.reinforced
                : r
                  ? !!r.movable
                  : !!socket;
            d.get('tertiary').text = 'Repair';
            d.get('tertiary').enabled = !!r && r.health < r.maxHealth;
        } else if (page === 'pack') {
            const slot = bag.slots[Number(this.selected)];
            text = slot
                ? `${ITEMS[slot.item].name}\n\n${ITEMS[slot.item].category.toUpperCase()}\n${slot.count} carried / ${ITEMS[slot.item].stack} per stack\n\n${ITEMS[slot.item].category === 'supplies' ? 'Fuel cans and repair kits are used through E → Vehicle controls.' : 'Select Use / equip to consume supplies or select a tool.'} Drop places one item in a physical bundle.\n\nEquipped: ${p.state.equipped === 'hands' ? 'Hands' : ITEMS[p.state.equipped].name}\nCoat: ${p.state.equipment.body ? ITEMS[p.state.equipment.body].name : 'None'}`
                : 'Your backpack is empty. Gather loose sticks, stones and plants with E.';
            d.get('primary').text = 'Use / equip';
        } else if (page === 'craft') {
            const recipe = RECIPES.find((r) => r.id === this.selected);
            text = recipe
                ? `${recipe.name}\n\n${recipe.description || 'Project-authored survival recipe.'}\n\n` +
                  Object.entries(recipe.ingredients)
                      .map(
                          ([id, n]) =>
                              `${ITEMS[id].name}  ${count(bag, id)} / ${n}`,
                      )
                      .join('\n') +
                  `\n\nStation: ${recipe.station === 'hand' ? 'Craft anywhere' : recipe.station === 'workbench' ? (p.housing.atStation('workbench') ? 'Workbench in reach' : 'Return to a workbench') : p.atFire() ? 'Campfire in reach' : 'Return to the campfire'}`
                : 'Choose a recipe.';
            d.get('primary').text = 'Craft selected';
        } else if (page === 'map') {
            text = destinationText(p, this.selected);
            d.get('primary').text = 'Track location';
        } else if (page === 'help')
            text =
                'MOVE  /  WASD + mouse · Shift sprint · Space jump · V first/third person\nWATER  /  Space swim up · Ctrl dive · refill your flask at the well\nINTERACT  /  E gathers loose resources, opens storage and speaks to people\nTOOLS & WEAPONS  /  Left click use/fire · right mouse aim · R reload\nEQUIPMENT  /  1–6 hotbar · Tab backpack, crafting and item actions\nBUILD  /  B catalogue · R rotates preview · click places · Escape cancels\nFURNISH  /  E on furniture opens move, storage, rest and dismantle actions\nTRAVEL  /  M locations and tracking · Vehicles & travel below explains driving\nSAVE  /  F6 save · F9 load · Escape pause\n\nMouse sensitivity: slider on the left. Field dressings treat fall injuries.';
        else if (page === 'travel')
            text =
                'BOARD  /  E on a vehicle, then Drive/Pilot · Tab cargo while stopped\nSERVICE  /  Carry sealed fuel cans and repair kits; use the vehicle menu\nROAD & BOAT  /  W/S forward/reverse · A/D steer · Space brake on land\nHELICOPTER  /  W/S forward/back · A/D yaw · Space climb · Ctrl descend\nRelease movement controls to return toward hover. Allow time for rotor spin-up.\nPLANE  /  W/S adjusts throttle · A/D turn · Space climb · Ctrl descend\nHold W for takeoff speed, then Space. S cuts throttle; Shift wheel brake.\nEXIT  /  E at low speed. Land aircraft first; boats prefer a dry pier.\nNAVIGATE  /  M tracks destinations without teleporting. Stop tracking restores journal.\nSAVE  /  F6, quit fully, then F9 after reopening to resume your expedition.\n\nHomestead vehicles are south behind camp. The helicopter is west of the apron.';
        else if (page === 'loading')
            text = navigation.errors.length
                ? 'The navigation bake is unavailable. Open the project in the editor and rebake Homestead / navigation. ' +
                  navigation.errors.map((e) => e.message).join(' / ')
                : 'Loading character art and preparing the island trails… Player actions and NPC routines begin after required navigation and character resources are ready.';
        else if (page === 'welcome')
            text =
                'A quiet coast. An island worth coming home to.\n\nSTART SMALL  /  E gathers sticks, stones and fibre near camp. Tab → Crafting makes an axe. Chop a tree, make a pickaxe, mine stone and cook beside the fire. Refill your flask at the well.\n\nMAKE A HOME  /  E opens furniture, storage and survivor conversations. B opens construction. Mara and Ivo trade and offer work.\n\nGO FURTHER  /  M lists seven destinations. Track one and follow the roads, or take a vehicle south behind camp. Detailed regions remember your changes.\n\nSAVE BEFORE LEAVING  /  F6 saves; F9 resumes. Controls above and On-foot controls below explain the next steps.';
        else if (page === 'dead')
            text =
                'Your expedition has ended. Recover at the homestead with your pack intact.\n\nRecovery keeps your pack intact; hunger, thirst, drowning and fall injuries are active.\n\nClaim a bed to choose your recovery home.';
        else
            text =
                'The world is paused. Your inventory, tools, harvested resources and physical state are included in a world save.\n\nSave before leaving. Loading replaces the current world with the last complete save.\n\nOpen Backpack, Crafting, Field map or Controls above.';
        d.get('detail').text = text;
        d.get('detail').setLayout({
            offset: storage
                ? { x: 24, y: 130 }
                : list
                  ? { x: 568, y: 132 }
                  : page === 'map'
                    ? { x: 668, y: 132 }
                    : { x: 24, y: 132 },
            size: storage
                ? { x: 1030, y: 28 }
                : list
                  ? { x: 500, y: 252 }
                  : page === 'map'
                    ? { x: 400, y: 350 }
                    : { x: 1030, y: 280 },
        });
        for (const id of [
            'close',
            'tab_pack',
            'tab_craft',
            'tab_build',
            'tab_map',
            'tab_help',
        ])
            d.get(id).enabled = page !== 'loading';
        d.get('load').enabled =
            page !== 'loading' && !p.refreshToken && saves.exists('island');
        d.get('save').enabled = page !== 'loading' && !p.request;
        d.get('quit').enabled = page !== 'loading' && !p.request;
    }
    update() {
        if (this.page === 'loading') {
            if (
                navigation.sample(this.p.entity.transform.position, 3) &&
                world.entity('6001').animation?.info?.ready &&
                world.entity('6201').animation?.info?.ready &&
                world.entity('6301').modelParts !== null &&
                Object.values(VEHICLES)
                    .filter((v) => !v.region)
                    .every((v) => world.entity(v.visual)?.modelParts !== null)
            )
                this.open('welcome');
            this.refresh();
            return;
        }
        const p = this.p;
        if (input.pressed('pause') && p.builder?.active) {
            p.builder.cancel();
            p.say('Placement cancelled.');
        } else if (input.pressed('pause'))
            this.modal ? this.close() : this.open('pause');
        else if (input.pressed('build_menu') && !p.transport.driving)
            this.open('build');
        else if (input.pressed('inventory')) {
            if (p.transport.driving) {
                if (this.page === 'storage') this.close();
                else p.transport.openCargo(p.state.transport.driving);
            } else this.page === 'pack' ? this.close() : this.open('pack');
        } else if (input.pressed('map'))
            this.page === 'map' ? this.close() : this.open('map');
        if (input.pressed('save')) p.requestSave();
        if (input.pressed('load')) p.requestLoad();
        p.pollSave();
        const v = p.state.vitals;
        this.doc.get('vitals').text =
            `HEALTH ${Math.ceil(v.health)}   FOOD ${Math.ceil(v.hunger)}   WATER ${Math.ceil(v.thirst)}\nSTAMINA ${Math.ceil(v.stamina)}${p.immersion > 0.5 ? '   AIR ' + Math.ceil(v.breath) + 's' : ''}${v.injury ? '   INJURY ' + v.injury : ''}`;
        this.doc.get('hotbar').text = p.state.hotbar
            .map(
                (id, i) =>
                    `${i + 1} ${id === p.state.equipped ? '[' : ''}${id === 'hands' ? 'HANDS' : { stone_axe: 'AXE', stone_pickaxe: 'PICK', berries: 'BERRIES', water: 'FLASK', bandage: 'DRESSING' }[id] || ITEMS[id].name.toUpperCase()}${id === p.state.equipped ? ']' : ''}${id === 'hands' ? '' : ' ×' + count(p.state.bag, id)}`,
            )
            .join('   ');
        this.doc.get('hotbar').text += this.p.combat?.status()
            ? '\n' + this.p.combat.status()
            : '';
        if (p.transport.driving)
            this.doc.get('hotbar').text = p.transport.status();
        this.doc.get('hint').text = p.transport.driving
            ? 'Mouse look · V change view · M field map · Escape pause · F6 save'
            : 'WASD move · Mouse look · E interact · Click tool · Tab backpack & crafting · Escape pause';
        this.doc.get('context').text = this.modal ? '' : p.context();
        this.doc.get('notice').text = p.notice;
        const s = p.state.stats;
        this.doc.get('journal').text = !s.crafted
            ? 'FIRST STEPS / Gather sticks, stone and fibre. Craft a stone axe.'
            : !s.felled
              ? 'TIMBER / Equip your axe and fell a tree.'
              : !s.mined
                ? 'FIELDWORK / Craft a pickaxe and mine stone or iron.'
                : !s.cooked
                  ? 'A WARM MEAL / Gather berries and cook beside the fire.'
                  : p.housing.state.home
                    ? 'HOME SWEET HOME / Store supplies, secure openings and improve your shelter.'
                    : 'CLAIM A HOME / Enter Seabird Cottage behind the trail sign. E on furniture opens its actions.';
        const q = p.state.combat?.quests;
        if (q?.patrol === 'active')
            this.doc.get('journal').text =
                'SUPPLY ROUTE / Eastern camp · scavengers ' +
                (p.state.combat.campKills || 0) +
                '/2. Return to Mara.';
        else if (q?.timber === 'active')
            this.doc.get('journal').text =
                'FIREWOOD / Bring six timber to Ivo at the camp.';
        if (guidance(p)) this.doc.get('journal').text = guidance(p);
        if (this.modal) this.refresh();
    }
    event(e) {
        if (this.page === 'loading') return;
        if (e.type === 'back') {
            this.close();
            return;
        }
        if (e.type === 'change' && e.element === 'entries') {
            this.selected = e.item;
            this.refresh();
        }
        if (e.type === 'change' && e.element === 'entries2') {
            this.containerSelected = e.item;
            this.refresh();
        }
        if (e.type === 'change' && e.element === 'sensitivity')
            this.p.state.sensitivity = 0.0007 + e.value * 0.004;
        if (e.type !== 'click') return;
        if (
            e.element === 'secondary' &&
            ['help', 'travel', 'welcome'].includes(this.page)
        ) {
            this.open(this.page === 'help' ? 'travel' : 'help');
            return;
        }
        if (e.element.startsWith('tab_')) {
            this.open(e.element.slice(4));
            return;
        }
        if (e.element === 'close') {
            if (this.page === 'dead') this.p.recover();
            this.close();
        }
        const p = this.p;
        if (vehicleEvent(this, e) || socialEvent(this, e)) return;
        if (this.page === 'map') {
            if (e.element === 'tertiary') {
                p.state.exploration.target = null;
                p.say(
                    'Location tracking stopped. Your journal is visible again.',
                );
                this.refresh();
                return;
            }
            if (e.element === 'primary') {
                p.state.exploration.target = this.selected;
                p.say(
                    'Tracking location. Follow the roads or take an aircraft.',
                );
                this.close();
            }
            if (e.element === 'secondary')
                this.referenceMap = !this.referenceMap;
        } else if (this.page === 'storage') {
            if (e.element === 'primary' || e.element === 'drop')
                (this.vehicleRef ? p.transport : p.housing).transfer(
                    this.containerRef,
                    Number(this.selected),
                    true,
                    e.element === 'drop',
                );
            if (e.element === 'secondary' || e.element === 'tertiary')
                (this.vehicleRef ? p.transport : p.housing).transfer(
                    this.containerRef,
                    Number(this.containerSelected),
                    false,
                    e.element === 'tertiary',
                );
        } else if (this.page === 'object') {
            const r = p.housing.state.objects[this.objectRef];
            if (e.element === 'primary') {
                if (r?.barricade) p.housing.repair(this.objectRef);
                else if (r) p.housing.use(this.objectRef);
                else if (p.housing.barricade(this.objectRef, false))
                    this.close();
            }
            if (e.element === 'secondary') {
                if (r?.barricade) p.housing.reinforce(this.objectRef);
                else if (r) p.builder.move(this.objectRef);
                else if (p.housing.barricade(this.objectRef, true))
                    this.close();
            }
            if (e.element === 'drop' && p.housing.remove(this.objectRef, true))
                this.close();
            if (e.element === 'tertiary') p.housing.repair(this.objectRef);
        } else if (e.element === 'primary') {
            if (this.page === 'build') p.builder.begin(this.selected);
            else if (this.page === 'craft') p.craft(this.selected);
            else p.useSlot(Number(this.selected));
        } else if (e.element === 'drop') p.dropSlot(Number(this.selected));
        if (e.element === 'save') this.p.requestSave();
        if (e.element === 'load') this.p.requestLoad();
        if (e.element === 'quit') {
            this.p.quitAfterSave = true;
            this.p.requestSave();
        }
    }
}
function worldPosition(p) {
    const t = p.entity.transform.position;
    return { x: t.x + WORLD.map.origin[0], z: t.z + WORLD.map.origin[1] };
}

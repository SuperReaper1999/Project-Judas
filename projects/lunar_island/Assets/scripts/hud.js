import { ui, input, saves, navigation, world } from 'judas';
import { ITEMS, RECIPES, CONFIG } from './definitions.js';
import { WORLD } from './world-data.js';
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
    openStorage(ref) {
        this.containerRef = ref;
        this.containerSelected = '';
        this.containerSignature = '';
        this.open('storage');
    }
    rows() {
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
        ].includes(page);
        const storage = page === 'storage',
            object = page === 'object' || socialPage(page);
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
        d.get('map_image').visible = page === 'map';
        d.get('sensitivity').visible = page === 'help';
        d.get('title').text =
            {
                loading: 'PREPARING THE HOMESTEAD',
                pack: 'FIELD PACK',
                build: 'CONSTRUCTION & FURNISHING',
                storage: 'STORAGE / LOOT',
                object: 'WORLD OBJECT',
                craft: 'CRAFTING',
                map: 'LUNAR ISLAND / FIELD MAP',
                help: 'CONTROLS & FIELD NOTES',
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
        if (socialPage(page)) text = renderSocial(this);
        else if (page === 'storage') {
            const bag2 = p.housing.state.containers[this.containerRef];
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
                ? `${ITEMS[slot.item].name}\n\n${ITEMS[slot.item].category.toUpperCase()}\n${slot.count} carried / ${ITEMS[slot.item].stack} per stack\n\nSelect Use / equip to consume supplies or select a tool. Drop places one item in a physical bundle.\n\nEquipped: ${p.state.equipped === 'hands' ? 'Hands' : ITEMS[p.state.equipped].name}\nCoat: ${p.state.equipment.body ? ITEMS[p.state.equipment.body].name : 'None'}`
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
        } else if (page === 'map')
            text =
                'STAGE 3 / HOMESTEAD COAST\n\nYou are in the north-eastern homestead clearing. The nearby spring pool, forest, Seabird Cottage and Birchside Cabin are playable.\n\nThe complete coast is terrain, but the labelled settlements and other regions are reference plans for Stage 5.\n\n' +
                `Absolute position\n${Math.round(worldPosition(p).x)} east / ${Math.round(worldPosition(p).z)} south`;
        else if (page === 'help')
            text =
                'WASD move · mouse look\nSpace jump / swim up · Ctrl dive\nShift sprint · V first/third person\nE gather / refill / interact\nLeft click use tool / confirm placement\nB construction · R rotate preview / reload gun\nE furniture / storage / secure opening\nRight mouse aims · Tab equips firearms\n1–6 hotbar\nTab backpack · M field map\nEscape pause / return\nF6 save · F9 load last save\n\nMouse sensitivity (slider left).\nCrafting and item actions are in this menu.\n\nThe spring well refills a flask; seawater is not drinkable.\nFall injuries slow movement; use a field dressing.';
        else if (page === 'loading')
            text = navigation.errors.length
                ? 'The navigation bake is unavailable. Open the project in the editor and rebake Homestead / navigation. ' +
                  navigation.errors.map((e) => e.message).join(' / ')
                : 'Loading character art and preparing the island trails… Player actions and NPC routines begin after required navigation and character resources are ready.';
        else if (page === 'welcome')
            text =
                'A quiet coast. An island worth coming home to.\n\nGather loose sticks, stones and fibre with E. Open Crafting to make an axe. Fell and process a tree, mine stone, cook at the campfire, and drink at the well.\n\nTwo enterable cottages now have real doors, loot, storage and functional furniture. E opens object actions. B opens construction. Mara and Ivo trade at camp. Ask Mara for a patrol job and sidearm; Ivo needs timber. Scavengers occupy the eastern trail; wolves roam west. Vehicles remain a later stage.\n\nSave with F6 before leaving.';
        else if (page === 'dead')
            text =
                'Your expedition has ended. Recover at the homestead with your pack intact.\n\nStage 1 keeps recovery forgiving; hunger, thirst, drowning and fall injuries are active.\n\nDeath penalties and progression will be balanced in later stages.';
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
                world.entity('6301').modelParts !== null
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
        else if (input.pressed('build_menu')) this.open('build');
        else if (input.pressed('inventory'))
            this.page === 'pack' ? this.close() : this.open('pack');
        else if (input.pressed('map'))
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
                p.state.combat.kills +
                '/2. Return to Mara.';
        else if (q?.timber === 'active')
            this.doc.get('journal').text =
                'FIREWOOD / Bring six timber to Ivo at the camp.';
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
        if (e.element.startsWith('tab_')) {
            this.open(e.element.slice(4));
            return;
        }
        if (e.element === 'close') {
            if (this.page === 'dead') this.p.recover();
            this.close();
        }
        const p = this.p;
        if (socialEvent(this, e)) return;
        if (this.page === 'storage') {
            if (e.element === 'primary' || e.element === 'drop')
                p.housing.transfer(
                    this.containerRef,
                    Number(this.selected),
                    true,
                    e.element === 'drop',
                );
            if (e.element === 'secondary' || e.element === 'tertiary')
                p.housing.transfer(
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

import { ui, input, saves } from 'judas';
import { ITEMS, RECIPES, CONFIG } from './definitions.js';
import { WORLD } from './world-data.js';
import { count, weight } from './inventory.js';
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
        else this.open('welcome');
    }
    get modal() {
        return !!this.page;
    }
    open(page = 'pack') {
        this.page = page;
        this.selected = '';
        this.lastRows = '';
        this.doc.modal = true;
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
    rows() {
        const bag = this.p.state.bag;
        if (this.page === 'pack')
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
        return [];
    }
    refresh() {
        const d = this.doc,
            p = this.p,
            bag = p.state.bag,
            page = this.page;
        d.get('overlay').visible = !!page;
        if (!page) return;
        const list = page === 'pack' || page === 'craft';
        d.get('entries').visible = list;
        d.get('primary').visible = list;
        d.get('drop').visible = page === 'pack';
        d.get('map_image').visible = page === 'map';
        d.get('sensitivity').visible = page === 'help';
        d.get('title').text =
            {
                pack: 'FIELD PACK',
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
        if (page === 'pack') {
            const slot = bag.slots[Number(this.selected)];
            text = slot
                ? `${ITEMS[slot.item].name}\n\n${ITEMS[slot.item].category.toUpperCase()}\n${slot.count} carried / ${ITEMS[slot.item].stack} per stack\n\nSelect Use / equip to consume supplies or select a tool. Drop places one item in a physical bundle.\n\nEquipped: ${p.state.equipped === 'hands' ? 'Hands' : ITEMS[p.state.equipped].name}\nCoat: ${p.state.equipment.body ? ITEMS[p.state.equipment.body].name : 'None'}`
                : 'Your backpack is empty. Gather loose sticks, stones and plants with E.';
            d.get('primary').text = 'Use / equip';
        } else if (page === 'craft') {
            const recipe = RECIPES.find((r) => r.id === this.selected);
            text = recipe
                ? `${recipe.name}\n\n${recipe.description}\n\n` +
                  Object.entries(recipe.ingredients)
                      .map(
                          ([id, n]) =>
                              `${ITEMS[id].name}  ${count(bag, id)} / ${n}`,
                      )
                      .join('\n') +
                  `\n\nStation: ${recipe.station === 'hand' ? 'Craft anywhere' : p.atFire() ? 'Campfire in reach' : 'Return to the campfire'}`
                : 'Choose a recipe.';
            d.get('primary').text = 'Craft selected';
        } else if (page === 'map')
            text =
                'STAGE 1 / HOMESTEAD COAST\n\nYou are in the north-eastern homestead clearing. The nearby spring pool, forest and stone trail are playable.\n\nThe complete coast is terrain, but the labelled settlements and other regions are reference plans for Stage 5.\n\n' +
                `Absolute position\n${Math.round(worldPosition(p).x)} east / ${Math.round(worldPosition(p).z)} south`;
        else if (page === 'help')
            text =
                'WASD move · mouse look\nSpace jump / swim up · Ctrl dive\nShift sprint · V first/third person\nE gather / refill / interact\nLeft click use equipped tool\n1–6 hotbar\nTab backpack · M field map\nEscape pause / return\nF6 save · F9 load last save\n\nMouse sensitivity (slider left).\nCrafting and item actions are in this menu.\n\nThe spring well refills a flask; seawater is not drinkable.\nFall injuries slow movement; use a field dressing.';
        else if (page === 'welcome')
            text =
                'A quiet coast. An island worth coming home to.\n\nGather loose sticks, stones and fibre with E. Open Crafting to make an axe. Fell and process a tree, mine stone, cook at the campfire, and drink at the well.\n\nThis is the first playable foundation. Buildings, NPCs, combat and vehicles come in later authorised stages.\n\nSave with F6 before leaving.';
        else if (page === 'dead')
            text =
                'Your expedition has ended. Recover at the homestead with your pack intact.\n\nStage 1 keeps recovery forgiving; hunger, thirst, drowning and fall injuries are active.\n\nDeath penalties and progression will be balanced in later stages.';
        else
            text =
                'The world is paused. Your inventory, tools, harvested resources and physical state are included in a world save.\n\nSave before leaving. Loading replaces the current world with the last complete save.\n\nOpen Backpack, Crafting, Field map or Controls above.';
        d.get('detail').text = text;
        d.get('detail').setLayout({
            offset: list
                ? { x: 568, y: 132 }
                : page === 'map'
                  ? { x: 668, y: 132 }
                  : { x: 24, y: 132 },
            size: list
                ? { x: 500, y: 252 }
                : page === 'map'
                  ? { x: 400, y: 350 }
                  : { x: 1030, y: 280 },
        });
        d.get('load').enabled = !p.refreshToken && saves.exists('island');
        d.get('save').enabled = !p.request;
        d.get('quit').enabled = !p.request;
    }
    update() {
        const p = this.p;
        if (input.pressed('pause'))
            this.modal ? this.close() : this.open('pause');
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
                  : 'A HOME TO RETURN TO / Explore the coast, manage supplies, and save your expedition.';
        if (this.modal) this.refresh();
    }
    event(e) {
        if (e.type === 'back') {
            this.close();
            return;
        }
        if (e.type === 'change' && e.element === 'entries') {
            this.selected = e.item;
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
        if (e.element === 'primary') {
            if (this.page === 'craft') this.p.craft(this.selected);
            else this.p.useSlot(Number(this.selected));
        }
        if (e.element === 'drop') this.p.dropSlot(Number(this.selected));
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

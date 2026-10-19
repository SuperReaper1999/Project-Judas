import { ui, input, saves } from 'judas';
import { recipes, kinds } from './inventory.js';
export default class HUD {
    constructor(p) { this.p = p; this.doc = ui.get('hearthworld') || ui.load('f1000000000000000000000000000002', 'hearthworld'); ui.debugOverlayVisible = false; this.mode = ''; this.open('welcome'); }
    get modal() { return !!this.mode; }
    open(mode) { this.mode = mode; this.doc.modal = true; input.pointerCapture = false; this.p.blocked = true; this.refresh(); }
    close() { this.mode = ''; this.doc.modal = false; input.pointerCapture = true; this.p.blocked = true; this.refresh(); }
    refresh() {
        const doc = this.doc, s = this.p.state, inventory = this.mode === 'inventory';
        doc.get('menu_bg').visible = !!this.mode && !inventory;
        doc.get('inventory_bg').visible = inventory;
        doc.get('start').visible = this.mode === 'welcome';
        doc.get('start').enabled = !this.p.refreshToken;
        doc.get('start').text = this.p.refreshToken ? 'CHECKING SAVED HOME…' : saves.exists('home') ? 'CONTINUE HOME' : 'START EXPLORING';
        doc.get('resume').visible = this.mode === 'pause';
        doc.get('menu_title').text = this.mode === 'welcome' ? 'HEARTHWORLD' : 'A MOMENT AT HOME';
        doc.get('menu_body').text = this.mode === 'welcome' ? 'A little planet. A place to call home.\n\nFind your three tools inside the cottage. Fell a tree, collect logs,\nmine stone, and build something of your own. Dig the backyard\ninto a pool with the shovel. Your world can be saved and revisited.' : 'Your world is paused. Save before leaving.\n\nF5 save  /  F9 load the last complete save\nSaving releases held objects without changing their motion.';
        doc.get('save').visible = this.mode === 'pause';
        doc.get('load').visible = !!this.mode && !inventory;
        doc.get('load').enabled = saves.exists('home');
        doc.get('inventory_body').text = `WOOD  ${s.inventory.wood}     STONE  ${s.inventory.stone}\n\nTOOLS\n${s.owned.axe ? '[owned]' : '[find in cottage]'} AXE    ${s.owned.pickaxe ? '[owned]' : '[find in cottage]'} PICKAXE\n${s.owned.shovel ? '[owned]' : '[find in cottage]'} SHOVEL    [owned] GRAVITY GUN\n\nCONSTRUCTION — choose with Q while holding the hammer\n` + kinds.map(k => `${k.toUpperCase()}   ${recipes[k].wood} wood / ${recipes[k].stone} stone`).join('\n') + '\n\n1 gravity gun   2 axe   3 pickaxe   4 shovel   5 hammer\nE collect    LMB use / place    RMB grab / release\nR turn held object / blueprint    V first / third person';
    }
    objective() { const s = this.p.state; if (!s.owned.axe || !s.owned.shovel || !s.owned.pickaxe)
        return 'MAKE YOURSELF AT HOME  /  Collect the three tools inside the cottage'; if (!s.stats.treesProcessed)
        return 'THE FIRST HARVEST  /  Fell a tree, then chop its fallen trunk'; if (s.stats.woodCollected < 9)
        return 'GOOD TIMBER  /  E collects each fallen log for 3 wood'; if (s.stats.stoneMined < 2)
        return 'FOUNDATIONS  /  Mine stone with the pickaxe'; if (s.stats.placed < 3)
        return 'YOUR OWN CORNER  /  Build three pieces with the hammer'; if (!s.stats.dug)
        return 'ROOM TO GROW  /  Dig a pool in the marked backyard'; return 'A PLACE TO RETURN TO  /  Explore, build, and save your homestead'; }
    update() {
        if (input.pressed('pause')) {
            if (this.modal)
                this.close();
            else
                this.open('pause');
        }
        else if (input.pressed('inventory')) {
            if (this.mode === 'inventory')
                this.close();
            else
                this.open('inventory');
        }
        if (input.pressed('save'))
            this.p.requestSave();
        if (input.pressed('load'))
            this.p.requestLoad();
        this.p.pollSave();
        const s = this.p.state;
        this.doc.get('resources').text = `WOOD ${s.inventory.wood}    STONE ${s.inventory.stone}    ${s.third ? 'THIRD' : 'FIRST'} PERSON`;
        this.doc.get('objective').text = this.objective();
        this.doc.get('notice').text = this.p.notice || '';
        this.doc.get('context').text = this.modal ? '' : this.p.context();
        this.doc.get('tools').text = ['1 GRAVITY', '2 AXE' + (!s.owned.axe ? ' [find]' : ''), '3 PICK' + (!s.owned.pickaxe ? ' [find]' : ''), '4 SHOVEL' + (!s.owned.shovel ? ' [find]' : ''), '5 BUILD'].map((v, i) => ['gravity', 'axe', 'pickaxe', 'shovel', 'build'][i] === s.equipped ? '[ ' + v + ' ]' : v).join('    ');
        if (this.modal)
            this.refresh();
    }
    event(e) { if (e.type !== 'click')
        return; if (e.element === 'start' || e.element === 'resume' || e.element === 'inventory_close')
        this.close(); if (e.element === 'save')
        this.p.requestSave(); if (e.element === 'load')
        this.p.requestLoad(); if (e.element === 'quit') {
        this.p.quitAfterSave = true;
        this.p.requestSave();
    } }
    destroy() { this.doc?.unload(); }
}

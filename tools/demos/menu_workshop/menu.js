import { world, ui, input, saves } from 'judas';
import { ITEMS } from './items.js';
export const properties = Object.fromEntries([...['previewCamera', 'previewActor', 'previewBlade', 'previewAxe', 'previewShield', 'previewPack'].map(k => [k, { type: 'entity' }]), ...['sword', 'axe', 'shield', 'wood', 'apple', 'ore', 'potion', 'pack'].map(k => [k, { type: 'string', default: '' }])]);
export default class Satchel {
    constructor(context = {}) {
        this.properties = context.properties ?? {};
        this.state = {
            version: 1, scale: 1, tab: 'inventory', health: 72, bag: Array(16).fill(null), near: [{ id: 'wood', n: 5 }, { id: 'apple', n: 3 }, { id: 'ore', n: 4 }, { id: 'pack', n: 1 }], equipment: {
                weapon: null, offhand: null, pack: null
            }, selected: 'bag0', angle: 0, open: true, request: null
        };
        this.message = 'Choose an item, or drag it to transfer. Escape resumes the world.';
    }
    start() {
        this.state.bag.splice(0, 7, ...['sword', 'apple', 'wood', 'ore', 'potion', 'axe', 'shield'].map((id, i) => ({ id, n: [1, 6, 12, 4, 2, 1, 1][i] })));
        this.connect();
    }
    restore() {
        this.state.request = null;
        this.connect();
    }
    connect() {
        this.doc = ui.get('satchel');
        this.camera = this.properties.previewCamera;
        this.actor = this.properties.previewActor;
        this.doc.get('preview').setPreviewCamera(this.camera);
        this.request = null;
        this.fonts = [...this.keys(), 'details', 'status', 'title', 'near_title', 'bag_title', 'weight', 'health', 'character_info', 'settings_info', 'resume', 'save', 'load', 'use', 'split', 'nearby', 'turn_left', 'turn_right', 'tab_inventory', 'tab_character', 'tab_settings', 'scale_small', 'scale_large', 'preview_label', 'weapon_caption', 'offhand_caption', 'pack_caption'].map(id => [id, this.doc.get(id).fontSize]);
        this.draw();
    }
    slot(key) {
        if (key.startsWith('bag'))
            return this.state.bag[Number(key.slice(3))];
        if (key.startsWith('near'))
            return this.state.near[Number(key.slice(4))];
        return this.state.equipment[key] ?? null;
    }
    setSlot(key, value) {
        if (key.startsWith('bag'))
            this.state.bag[Number(key.slice(3))] = value;
        else if (key.startsWith('near'))
            this.state.near[Number(key.slice(4))] = value;
        else
            this.state.equipment[key] = value;
    }
    keys() {
        return [...this.state.bag.map((_, i) => 'bag' + i), ...this.state.near.map((_, i) => 'near' + i), 'weapon', 'offhand', 'pack'];
    }
    fits(key, item) {
        return !item || key.startsWith('bag') || key.startsWith('near') || ITEMS[item.id].slot === key;
    }
    weight() {
        return [...this.state.bag, ...Object.values(this.state.equipment)].reduce((n, x) => n + (x ? ITEMS[x.id].weight * x.n : 0), 0);
    }
    transfer(from, to) {
        if (from === to)
            return;
        const a = this.slot(from), b = this.slot(to);
        if (!a)
            return;
        if (!this.fits(to, a) || !this.fits(from, b)) {
            this.message = 'That equipment slot does not accept this item.';
            return;
        }
        const previous = JSON.stringify({
            bag: this.state.bag, near: this.state.near, equipment: this.state.equipment
        });
        if (b && b.id === a.id && ITEMS[a.id].max > 1) {
            const moved = Math.min(a.n, ITEMS[a.id].max - b.n);
            if (!moved) {
                this.message = 'That stack is full.';
                return;
            }
            this.setSlot(to, { id: a.id, n: b.n + moved });
            this.setSlot(from, a.n > moved ? { id: a.id, n: a.n - moved } : null);
        }
        else {
            this.setSlot(from, b);
            this.setSlot(to, a);
        }
        if (this.weight() > 35) {
            Object.assign(this.state, JSON.parse(previous));
            this.message = 'Too heavy: carry limit is 35 kg.';
            return;
        }
        this.state.selected = to;
        this.message = ITEMS[a.id].name + ' transferred. Inventory rules ran in project JavaScript.';
    }
    use() {
        const key = this.state.selected, item = this.slot(key);
        if (!item)
            return;
        const data = ITEMS[item.id];
        if (data.heal) {
            this.state.health = Math.min(100, this.state.health + data.heal);
            this.setSlot(key, item.n === 1 ? null : { id: item.id, n: item.n - 1 });
            this.message = 'Used ' + data.name + '.';
        }
        else if (data.slot)
            this.transfer(key, data.slot);
        else
            this.message = 'A crafting resource. Try transferring or splitting its stack.';
    }
    split() {
        const key = this.state.selected;
        if (!key.startsWith('bag')) {
            this.message = 'Take the stack into your bag before splitting it.';
            return;
        }
        const item = this.slot(key), index = this.state.bag.indexOf(null);
        if (!item || item.n < 2 || index < 0) {
            this.message = 'Select a stack of two or more; a free bag slot is required.';
            return;
        }
        const n = Math.floor(item.n / 2);
        this.setSlot(key, { id: item.id, n: item.n - n });
        this.state.bag[index] = { id: item.id, n };
        this.message = 'Stack split without creating or losing items.';
    }
    onUI(e) {
        if (e.document !== 'satchel')
            return;
        if (e.type === 'back') {
            if (!this.openedThisFrame)
                this.state.open = false;
        }
        else if (e.type === 'drop' && e.sourceDocument === 'satchel' && this.keys().includes(e.sourceElement) && this.keys().includes(e.element))
            this.transfer(e.sourceElement, e.element);
        else if ((e.type === 'dragStart' || e.type === 'click' || e.type === 'focus') && this.keys().includes(e.element))
            this.state.selected = e.element;
        else if (e.type === 'click') {
            if (e.element === 'resume')
                this.state.open = false;
            else if (e.element === 'use')
                this.use();
            else if (e.element === 'split')
                this.split();
            else if (e.element === 'nearby') {
                const i = this.state.near.indexOf(null);
                if (i >= 0)
                    this.transfer(this.state.selected, 'near' + i);
                else
                    this.message = 'Nearby chest is full.';
            }
            else if (e.element === 'save' && !saves.busy)
                this.state.request = saves.save('satchel');
            else if (e.element === 'load' && !saves.busy)
                this.state.request = saves.load('satchel');
            else if (e.element === 'turn_left')
                this.state.angle -= .35;
            else if (e.element === 'turn_right')
                this.state.angle += .35;
            else if (e.element.startsWith('tab_')) {
                this.state.tab = e.element.slice(4);
                this.message = this.state.tab === 'inventory' ? 'Inventory and equipment' : this.state.tab === 'character' ? 'Character | inspect your traveller and equipped blade.' : 'Settings | UI scale controls are below; project input bindings remain project-owned.';
            }
            else if (e.element === 'scale_small')
                this.state.scale = Math.max(.85, this.state.scale - .1);
            else if (e.element === 'scale_large')
                this.state.scale = Math.min(1.2, this.state.scale + .1);
        }
        this.draw();
    }
    uiUpdate(dt) {
        this.openedThisFrame = false;
        if (!this.state.open && input.pressed('pause')) {
            this.state.open = true;
            this.openedThisFrame = true;
        }
        if (this.state.open) {
            if (input.held('ui_left'))
                this.state.angle -= dt * 1.2;
            if (input.held('ui_right'))
                this.state.angle += dt * 1.2;
        }
        if (this.state.request) {
            const r = saves.status(this.state.request);
            if (r)
                this.message = 'Save system: ' + r.state + (r.error ? ' | ' + r.error : '');
        }
        this.draw();
    }
    draw() {
        if (!this.doc)
            return;
        if (this.lastOpen !== this.state.open) {
            this.doc.visible = true;
            this.doc.get('menu').visible = this.state.open;
            this.doc.get('hint').visible = !this.state.open;
            this.doc.modal = this.state.open;
            this.camera.setCameraEnabled(this.state.open);
            this.lastOpen = this.state.open;
        }
        if (this.fontScale !== this.state.scale) {
            for (const [id, size] of this.fonts)
                this.doc.get(id).configure({ fontSize: size * this.state.scale });
            this.fontScale = this.state.scale;
        }
        if (this.lastTab !== this.state.tab) {
            for (const tab of ['inventory', 'character', 'settings'])
                this.doc.get(tab + '_group').visible = this.state.tab === tab;
            this.lastTab = this.state.tab;
        }
        this.drawPreview();
        const signature = JSON.stringify([this.state.bag, this.state.near, this.state.equipment, this.state.selected, this.state.health, this.state.tab, this.message]);
        if (signature === this.uiSignature)
            return;
        this.uiSignature = signature;
        for (const key of this.keys()) {
            const item = this.slot(key), element = this.doc.get(key);
            element.text = item ? ITEMS[item.id].name.split(' ')[0] + (item.n > 1 ? ' x' + item.n : '') : '—';
            element.configure({
                dragType: item ? 'item' : '', dropType: 'item', background: key === this.state.selected ? {
                    r: .24, g: .19, b: .1, a: 1
                } : {
                    r: .07, g: .075, b: .065, a: 1
                }
            });
            const icon = item ? this.properties[ITEMS[item.id].icon] : '';
            element.texture = icon;
            this.doc.get(key + '_icon').texture = icon;
        }
        const selected = this.slot(this.state.selected);
        this.doc.get('details').text = selected ? ITEMS[selected.id].name + ' | ' + selected.n + ' | ' + (ITEMS[selected.id].weight * selected.n).toFixed(1) + ' kg' : 'Select an item to inspect it.';
        this.doc.get('status').text = this.message;
        this.doc.get('weight').text = this.weight().toFixed(1) + ' / 35.0 kg';
        this.doc.get('health').text = 'Health ' + this.state.health + ' / 100';
        this.doc.get('health_bar').value = this.state.health / 100;
    }
    drawPreview() {
        const signature = JSON.stringify([this.state.angle, this.state.equipment]);
        if (signature === this.previewSignature)
            return;
        this.previewSignature = signature;
        this.actor.transform = { position: {
                x: 50, y: 0, z: 0
            }, rotation: {
                w: Math.cos(this.state.angle / 2), x: 0, y: Math.sin(this.state.angle / 2), z: 0
            } };
        const equipment = this.state.equipment;
        for (const [key, shown, local] of [['previewBlade', equipment.weapon?.id === 'sword', {
                    x: .46, y: .83, z: 0
                }], ['previewAxe', equipment.weapon?.id === 'axe', {
                    x: .46, y: .83, z: 0
                }], ['previewShield', !!equipment.offhand, {
                    x: -.46, y: 1, z: .12
                }], ['previewPack', !!equipment.pack, {
                    x: 0, y: 1.1, z: -.25
                }]]) {
            const mesh = this.properties[key];
            mesh.renderVisible = shown;
            mesh.transform = { position: {
                    x: 50 + local.x * Math.cos(this.state.angle) + local.z * Math.sin(this.state.angle), y: local.y, z: local.z * Math.cos(this.state.angle) - local.x * Math.sin(this.state.angle)
                }, rotation: {
                    w: Math.cos(this.state.angle / 2), x: 0, y: Math.sin(this.state.angle / 2), z: 0
                } };
        }
    }
    presentationUpdate() {
        world.setView({ position: {
                x: 0, y: 2, z: 6
            }, rotation: {
                w: .993, x: -.118, y: 0, z: 0
            } }, 60);
    }
    destroy() {
        if (this.camera?.valid)
            this.camera.setCameraEnabled(false);
    }
}

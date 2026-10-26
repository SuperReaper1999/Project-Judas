import assert from 'node:assert/strict';
import * as Bag from '../Assets/scripts/inventory.js';
import { ITEMS, RECIPES } from '../Assets/scripts/definitions.js';
import {
    initialVitals,
    tick,
    consume,
    fall,
} from '../Assets/scripts/survival.js';
let checks = 0;
function check(value, message) {
    checks++;
    assert.ok(value, message);
    console.log('PASS', message);
}
const bytes = (x) => JSON.stringify(x);
let bag = Bag.createBag(2, 100),
    before = bytes(bag),
    r = Bag.add(bag, 'wood', 35);
check(
    r.ok && r.bag.slots[0].count === 30 && r.bag.slots[1].count === 5,
    'stacking splits at the authored limit',
);
check(bytes(bag) === before, 'successful operations return a new bag');
bag = r.bag;
check(
    Bag.weight(bag) === 21 && Bag.count(bag, 'wood') === 35,
    'weight and quantity agree with actual stacks',
);
for (const n of [0, -1, 1.5, NaN, Infinity, Number.MAX_SAFE_INTEGER + 1]) {
    check(
        !Bag.add(bag, 'wood', n).ok,
        'invalid insertion quantity rejects: ' + n,
    );
    check(
        !Bag.remove(bag, 'wood', n).ok,
        'invalid withdrawal quantity rejects: ' + n,
    );
}
before = bytes(bag);
check(
    !Bag.add(bag, 'stone', 1).ok && bytes(bag) === before,
    'full-slot failure is atomic',
);
check(
    !Bag.remove(bag, 'wood', 36).ok && bytes(bag) === before,
    'insufficient quantity is atomic',
);
check(!Bag.add(bag, 'unknown', 1).ok, 'unknown item rejects');
check(
    !Bag.add(Bag.createBag(10, 0.1), 'stone', 1).ok,
    'weight limit is enforced independently of slots',
);
let crafting = Bag.createBag();
for (const [id, n] of Object.entries({
    wood: 10,
    stone: 10,
    fiber: 10,
    berries: 8,
}))
    crafting = Bag.add(crafting, id, n).bag;
before = bytes(crafting);
r = Bag.craft(crafting, 'stone_axe');
check(
    r.ok &&
        Bag.count(r.bag, 'stone_axe') === 1 &&
        Bag.count(r.bag, 'wood') === 7 &&
        Bag.count(r.bag, 'stone') === 8 &&
        Bag.count(r.bag, 'fiber') === 8,
    'actual axe recipe consumes exact ingredients and creates one tool',
);
check(bytes(crafting) === before, 'crafting does not mutate its input');
check(
    !Bag.craft(crafting, 'cooked_meal').ok,
    'cooking refuses the wrong station',
);
check(
    Bag.craft(crafting, 'cooked_meal', 'campfire').ok,
    'cooking succeeds beside the proper station',
);
check(
    !Bag.craft(Bag.createBag(), 'stone_pickaxe').ok,
    'missing ingredients refuse crafting',
);
let tiny = Bag.add(Bag.createBag(1, 100), 'wood', 1).bag;
before = bytes(tiny);
check(
    !Bag.exchange(tiny, {}, { stone: 1 }).ok && bytes(tiny) === before,
    'failed output insertion leaves every original item intact',
);
const source = Bag.add(Bag.createBag(), 'stone', 4).bag,
    dest = Bag.createBag(1, 0.1);
before = bytes(source);
check(
    !Bag.transfer(source, dest, 'stone', 1).ok && bytes(source) === before,
    'failed transfer never debits the source',
);
r = Bag.transfer(source, Bag.createBag(), 'stone', 2);
check(
    r.ok && Bag.count(r.from, 'stone') === 2 && Bag.count(r.to, 'stone') === 2,
    'shared container transfer preserves quantity',
);
r = Bag.exchange(
    Bag.add(Bag.createBag(1, 2), 'water', 1).bag,
    { water: 1 },
    { empty_flask: 1 },
);
check(
    r.ok && Bag.count(r.bag, 'empty_flask') === 1,
    'consumable container returns in the freed slot',
);
for (const recipe of RECIPES) {
    check(
        ['hand', 'campfire', 'workbench'].includes(recipe.station),
        'station has authored meaning',
    );
    for (const id of [
        ...Object.keys(recipe.ingredients),
        ...Object.keys(recipe.outputs),
    ])
        check(!!ITEMS[id], 'recipe references registered item ' + id);
}
let v = initialVitals();
tick(v, 10, { sprinting: true });
check(
    v.stamina === 0 && v.hunger < 75 && v.thirst < 65,
    'sprint and survival needs advance through actual rates',
);
tick(v, 2);
check(v.stamina === 28, 'rest restores bounded stamina');
v.health = 50;
v.injury = 2;
const healed = consume(v, { health: 25, injury: -1 });
check(
    healed.health === 75 && healed.injury === 1 && v.health === 50,
    'field dressing heals and treats an injury without mutating input',
);
v = initialVitals();
check(!fall(v, 8) && v.health === 100, 'ordinary landing does no damage');
check(
    fall(v, 12) && v.health === 88 && v.injury === 1,
    'hard landing applies injury and health damage',
);
v.hunger = 0;
v.thirst = 0;
const hp = v.health;
tick(v, 2);
check(v.health < hp, 'unmet needs cause real health loss');
v = initialVitals();
for (let i = 0; i < 25; i++) tick(v, 1, { underwater: true });
check(
    v.breath === 0 && v.health < 100,
    'submersion exhausts breath and causes damage',
);
const saved = JSON.parse(JSON.stringify({ bag: r.bag, vitals: v }));
check(
    saved.bag.slots[0].item === 'empty_flask' && saved.vitals.breath === 0,
    'foundation state is plain cold-save JSON',
);

let work = Bag.createBag();
work = Bag.add(work, 'scrap', 6).bag;
check(
    !Bag.craft(work, 'metal_sheet', 'hand').ok,
    'metal sheet cannot be crafted away from a workbench',
);
check(
    Bag.craft(work, 'metal_sheet', 'workbench').ok,
    'workbench sheet recipe accepts the actual station',
);
check(
    !Bag.transfer(work, work, 'scrap', 1).ok,
    'same-container transfer rejects rather than duplicating items',
);

console.log(`Lunar Island pure gameplay: ${checks} checks passed`);

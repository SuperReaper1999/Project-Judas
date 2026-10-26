import assert from 'node:assert/strict';
import * as Bag from '../Assets/scripts/inventory.js';
import * as Rules from '../Assets/scripts/encounter-rules.js';
import { PEOPLE, WEAPONS } from '../Assets/scripts/encounter-data.js';
let checks = 0;
const check = (ok, name) => {
    assert.ok(ok, name);
    checks++;
    console.log('PASS', name);
};
const stocked = (items) => {
    let b = Bag.createBag(18, 100);
    for (const [id, n] of Object.entries(items)) {
        const r = Bag.add(b, id, n);
        assert.ok(r.ok);
        b = r.bag;
    }
    return b;
};
let player = stocked({ tokens: 20 }),
    vendor = stocked({ tokens: 40, pistol: 2, ammo_pistol: 30 }),
    before = JSON.stringify([player, vendor]);
let t = Rules.trade(player, vendor, 'pistol', 12, true);
check(
    t.ok &&
        Bag.count(t.player, 'pistol') === 1 &&
        Bag.count(t.player, 'tokens') === 8 &&
        Bag.count(t.vendor, 'tokens') === 52 &&
        Bag.count(t.vendor, 'pistol') === 1,
    'purchase conserves stock and currency across two bags',
);
check(
    JSON.stringify([player, vendor]) === before,
    'trade never mutates either input',
);
let back = Rules.trade(t.player, t.vendor, 'pistol', 6, false);
check(
    back.ok &&
        Bag.count(back.player, 'pistol') === 0 &&
        Bag.count(back.vendor, 'pistol') === 2 &&
        Bag.count(back.player, 'tokens') === 14,
    'sale reverses goods with authored resale price',
);
for (const price of [0, -1, NaN, Infinity, 1.5])
    check(
        !Rules.trade(player, vendor, 'pistol', price).ok,
        'invalid offer refuses ' + price,
    );
check(
    !Rules.trade(player, vendor, 'pistol', 21).ok,
    'insufficient player money refuses',
);
check(
    !Rules.trade(player, stocked({ tokens: 40 }), 'pistol', 12).ok,
    'out of stock refuses',
);
check(
    !Rules.trade(
        stocked({ pistol: 1 }),
        stocked({ tokens: 1 }),
        'pistol',
        6,
        false,
    ).ok,
    'vendor cannot spend absent money',
);
check(
    !Rules.trade(player, vendor, 'tokens', 1).ok,
    'currency cannot be traded for itself',
);
const full = Bag.add(Bag.createBag(1, 100), 'tokens', 20).bag;
check(
    !Rules.trade(full, vendor, 'pistol', 12).ok,
    'destination capacity rejects a purchase atomically',
);
check(
    JSON.stringify([player, vendor]) === before,
    'failed offers leave both original ledgers intact',
);
let bag = stocked({ pistol: 1, ammo_pistol: 24 });
let reload = Rules.reload(bag, WEAPONS.pistol, 0);
check(
    reload.ok &&
        reload.magazine === 10 &&
        Bag.count(reload.bag, 'ammo_pistol') === 14,
    'reload transfers reserve rounds into finite magazine',
);
check(Bag.count(bag, 'ammo_pistol') === 24, 'reload does not mutate source');
reload = Rules.reload(stocked({ ammo_pistol: 3 }), WEAPONS.pistol, 2);
check(
    reload.ok &&
        reload.magazine === 5 &&
        Bag.count(reload.bag, 'ammo_pistol') === 0,
    'partial reload uses only existing rounds',
);
check(
    !Rules.reload(bag, WEAPONS.pistol, 10).ok,
    'full magazine does not spend ammo',
);
check(
    !Rules.reload(stocked({ ammo_shell: 2 }), WEAPONS.pistol, 0).ok,
    'wrong ammunition never reloads',
);
check(
    !Rules.reload(bag, WEAPONS.pistol, 11).ok,
    'overcapacity state is not repaired by reload',
);
const reward = Rules.reward(
    stocked({ wood: 6 }),
    { wood: 6 },
    { tokens: 15, rations: 2 },
);
check(
    reward.ok &&
        Bag.count(reward.bag, 'wood') === 0 &&
        Bag.count(reward.bag, 'tokens') === 15,
    'quest transaction consumes real supplies and grants authored reward',
);
check(
    !Rules.reward(stocked({ wood: 5 }), { wood: 6 }, { tokens: 15 }).ok,
    'incomplete quest cannot spend or grant',
);
check(
    Object.values(WEAPONS).every(
        (w) => w.damage > 0 && w.range > 0 && w.interval > 0,
    ),
    'every weapon has bounded positive gameplay values',
);
check(
    Object.values(WEAPONS)
        .filter((w) => w.ammo)
        .every(
            (w) =>
                Number.isInteger(w.capacity) && w.capacity > 0 && w.reload > 0,
        ),
    'firearm capacities and reload times are explicit',
);
check(
    !Rules.trade(player, player, 'pistol', 1).ok,
    'same container cannot trade with itself',
);
check(
    !Rules.reload(bag, WEAPONS.pistol, -1).ok,
    'negative magazine state rejects',
);
const batch = Rules.trade(player, vendor, 'ammo_pistol', 1, true, 10);
check(
    batch.ok &&
        Bag.count(batch.player, 'ammo_pistol') === 10 &&
        Bag.count(batch.vendor, 'ammo_pistol') === 20 &&
        Bag.count(batch.player, 'tokens') === 10,
    'bulk trade conserves ten goods and complete price',
);
check(
    !Rules.trade(player, vendor, 'ammo_pistol', 1, true, 31).ok,
    'insufficient bulk stock refuses atomically',
);
check(
    !Rules.trade(player, vendor, 'ammo_pistol', 1, true, NaN).ok,
    'invalid batch quantity rejects',
);
console.log(checks + ' encounter checks passed');

const patrol = PEOPLE.quests.patrol;
let q = Rules.quest(stocked({}), null, { campKills: 0 }, patrol);
check(
    q.ok &&
        q.stage === 'active' &&
        Bag.count(q.bag, 'pistol') === 1 &&
        Bag.count(q.bag, 'ammo_pistol') === 24,
    'accepting patrol grants its finite starter kit',
);
let again = Rules.quest(q.bag, q.stage, { campKills: 0 }, patrol);
check(
    again.ok && again.bag === q.bag && again.stage === 'active',
    'active patrol cannot repeat the starter gift',
);
let complete = Rules.quest(q.bag, q.stage, { campKills: 2 }, patrol);
check(
    complete.ok &&
        complete.stage === 'done' &&
        Bag.count(complete.bag, 'tokens') === 35,
    'actual objective counter unlocks authored reward',
);
check(
    Rules.quest(complete.bag, 'done', { campKills: 2 }, patrol).bag ===
        complete.bag,
    'completed quest does not repeat payment',
);
check(
    !Rules.quest(Bag.createBag(1, 1), null, { campKills: 0 }, patrol).ok,
    'starter gift failure is atomic when pack cannot hold it',
);
const timber = PEOPLE.quests.timber;
check(
    Rules.quest(stocked({ wood: 5 }), null, {}, timber).stage === 'active',
    'delivery quest remains active without required supplies',
);
q = Rules.quest(stocked({ wood: 6 }), null, {}, timber);
check(
    q.ok &&
        q.stage === 'done' &&
        Bag.count(q.bag, 'wood') === 0 &&
        Bag.count(q.bag, 'tokens') === 15,
    'data-driven delivery exchanges exact physical resource quantities',
);
console.log(checks + ' total encounter checks passed');

check(
    Rules.quest(q.bag, 'active', { kills: 5, campKills: 0 }, patrol).stage ===
        'active',
    'unrelated regional kills cannot satisfy the specific homestead patrol',
);

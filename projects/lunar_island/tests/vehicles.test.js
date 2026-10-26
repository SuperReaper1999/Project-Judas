import assert from 'node:assert/strict';
import * as Bag from '../Assets/scripts/inventory.js';
import { service, burn } from '../Assets/scripts/vehicle-rules.js';
import { VEHICLES } from '../Assets/scripts/vehicle-data.js';
let checks = 0;
const check = (ok, name) => {
    assert.ok(ok, name);
    checks++;
    console.log('PASS', name);
};
let bag = Bag.add(Bag.createBag(18, 100), 'fuel_can', 2).bag;
bag = Bag.add(bag, 'repair_kit', 2).bag;
const state = { fuel: 20, health: 40, bag: Bag.createBag(30, 650), burned: 0 };
const before = JSON.stringify([bag, state]);
let r = service(bag, state, VEHICLES.truck, 'fuel');
check(
    r.ok && r.state.fuel === 25 && Bag.count(r.bag, 'fuel_can') === 1,
    'sealed can transfers exactly 5 L and consumes one real inventory item',
);
check(
    JSON.stringify([bag, state]) === before,
    'service is immutable before caller commits',
);
check(
    !service(bag, { ...state, fuel: 68 }, VEHICLES.truck, 'fuel').ok,
    'insufficient tank space refuses without spilling part of a sealed can',
);
check(
    !service(Bag.createBag(), state, VEHICLES.truck, 'fuel').ok,
    'no can means no fuel',
);
r = service(bag, state, VEHICLES.truck, 'repair');
check(
    r.ok && r.state.health === 75 && Bag.count(r.bag, 'repair_kit') === 1,
    'repair consumes a kit and restores 35 condition',
);
check(
    service(bag, { ...state, health: 95 }, VEHICLES.truck, 'repair').state
        .health === 100,
    'repair does not exceed healthy condition',
);
check(
    !service(bag, { ...state, health: 100 }, VEHICLES.truck, 'repair').ok,
    'healthy vehicle does not waste a kit',
);
for (const fuel of [-1, NaN, Infinity, 71])
    check(
        !service(bag, { ...state, fuel }, VEHICLES.truck, 'fuel').ok,
        'invalid tank state rejects ' + fuel,
    );
let running = { ...state };
for (let i = 0; i < 10000; i++) running = burn(running, 0.003);
check(
    running.fuel === 0 && Math.abs(running.burned - 20) < 1e-9,
    'repeated fuel burn stops at zero and preserves used+remaining quantity',
);
for (const n of [-1, NaN, Infinity]) {
    assert.throws(() => burn(state, n));
    checks++;
}
const truck = Bag.add(
    Bag.createBag(VEHICLES.truck.slots, VEHICLES.truck.capacity),
    'wood',
    25,
).bag;
r = Bag.transfer(truck, Bag.createBag(18, 32), 'wood', 6);
check(
    r.ok && Bag.count(r.from, 'wood') === 19 && Bag.count(r.to, 'wood') === 6,
    'vehicle and personal inventories use identical transfer rules',
);
check(
    !Bag.transfer(truck, Bag.createBag(1, 0.01), 'wood', 1).ok,
    'overweight cargo withdrawal refuses atomically',
);
check(
    !Bag.transfer(truck, truck, 'wood', 1).ok,
    'same-container cargo transfer cannot duplicate items',
);
check(
    [
        'car',
        'truck',
        'motorcycle',
        'boat',
        'helicopter',
        'plane',
        'garage_truck',
    ].every((key) => VEHICLES[key]?.kind !== undefined),
    'six vehicle types plus a genuinely streamed service pickup are authored',
);
console.log(checks + ' vehicle checks passed');

check(
    VEHICLES.region_harbour_boat.region === 'harbour' &&
        VEHICLES.region_islet_boat.region === 'islet',
    'ocean skiffs are authored in their own persistent districts',
);
console.log(checks + ' total vehicle checks passed');

// Pure, reusable bag operations. Failed transactions never modify the caller.
import { ITEMS, RECIPES, CONFIG } from './definitions.js';
export function createBag(slots = CONFIG.slots, maxWeight = CONFIG.maxWeight) {
    return { slots: Array(slots).fill(null), maxWeight };
}
export function count(bag, id) {
    return bag.slots.reduce((n, s) => n + (s?.item === id ? s.count : 0), 0);
}
export function weight(bag) {
    return bag.slots.reduce(
        (n, s) => n + (s ? ITEMS[s.item].weight * s.count : 0),
        0,
    );
}
export function copy(bag) {
    return {
        slots: bag.slots.map((s) => (s ? { ...s } : null)),
        maxWeight: bag.maxWeight,
    };
}
function validAmount(n) {
    return Number.isSafeInteger(n) && n > 0;
}
export function add(bag, id, amount) {
    const def = ITEMS[id];
    if (!def || !validAmount(amount))
        return { ok: false, reason: 'Invalid item quantity.' };
    if (weight(bag) + def.weight * amount > bag.maxWeight + 1e-9)
        return { ok: false, reason: 'Too heavy. Drop or use something first.' };
    const next = copy(bag);
    let remaining = amount;
    for (const slot of next.slots)
        if (slot?.item === id) {
            const n = Math.min(remaining, def.stack - slot.count);
            slot.count += n;
            remaining -= n;
        }
    for (let i = 0; i < next.slots.length && remaining; i++)
        if (!next.slots[i]) {
            const n = Math.min(remaining, def.stack);
            next.slots[i] = { item: id, count: n };
            remaining -= n;
        }
    return remaining
        ? { ok: false, reason: 'Your backpack is full.' }
        : { ok: true, bag: next };
}
export function remove(bag, id, amount) {
    if (!ITEMS[id] || !validAmount(amount) || count(bag, id) < amount)
        return {
            ok: false,
            reason: 'Not enough ' + (ITEMS[id]?.name || id) + '.',
        };
    const next = copy(bag);
    let remaining = amount;
    for (let i = next.slots.length - 1; i >= 0 && remaining; i--) {
        const s = next.slots[i];
        if (s?.item === id) {
            const n = Math.min(s.count, remaining);
            s.count -= n;
            remaining -= n;
            if (!s.count) next.slots[i] = null;
        }
    }
    return { ok: true, bag: next };
}
export function exchange(bag, cost, output) {
    let next = bag;
    for (const [id, n] of Object.entries(cost)) {
        const r = remove(next, id, n);
        if (!r.ok) return r;
        next = r.bag;
    }
    for (const [id, n] of Object.entries(output)) {
        const r = add(next, id, n);
        if (!r.ok) return r;
        next = r.bag;
    }
    return { ok: true, bag: next };
}
export function craft(bag, recipeId, station = 'hand') {
    const recipe = RECIPES.find((r) => r.id === recipeId);
    if (!recipe) return { ok: false, reason: 'Unknown recipe.' };
    if (recipe.station !== 'hand' && station !== recipe.station)
        return {
            ok: false,
            reason: 'Stand beside a homestead campfire to cook.',
        };
    return exchange(bag, recipe.ingredients, recipe.outputs);
}
export function transfer(from, to, id, amount) {
    const debit = remove(from, id, amount);
    if (!debit.ok) return debit;
    const credit = add(to, id, amount);
    if (!credit.ok) return credit;
    return { ok: true, from: debit.bag, to: credit.bag };
}

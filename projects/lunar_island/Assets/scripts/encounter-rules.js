// Pure transactions shared by dialogue, trade, corpse loot and combat tests.
import * as Bag from './inventory.js';
export function trade(player, vendor, item, price, buy = true, amount = 1) {
    if (
        player === vendor ||
        !Number.isSafeInteger(amount) ||
        amount <= 0 ||
        !Number.isSafeInteger(price * amount) ||
        !Number.isSafeInteger(price) ||
        price <= 0 ||
        item === 'tokens'
    )
        return { ok: false, reason: 'Invalid trade offer.' };
    const buyer = buy ? player : vendor,
        seller = buy ? vendor : player;
    const debit = Bag.exchange(
        buyer,
        { tokens: price * amount },
        { [item]: amount },
    );
    if (!debit.ok) return debit;
    const credit = Bag.exchange(
        seller,
        { [item]: amount },
        { tokens: price * amount },
    );
    if (!credit.ok) return credit;
    return {
        ok: true,
        player: buy ? debit.bag : credit.bag,
        vendor: buy ? credit.bag : debit.bag,
    };
}
export function reload(bag, weapon, magazine) {
    if (
        !Number.isSafeInteger(magazine) ||
        magazine < 0 ||
        magazine > weapon.capacity
    )
        return { ok: false, reason: 'Invalid magazine state.' };
    const amount = Math.min(
        weapon.capacity - magazine,
        Bag.count(bag, weapon.ammo),
    );
    if (!Number.isSafeInteger(amount) || amount <= 0)
        return {
            ok: false,
            reason: 'No compatible reserve ammunition or magazine already full.',
        };
    const result = Bag.remove(bag, weapon.ammo, amount);
    return result.ok
        ? { ok: true, bag: result.bag, magazine: magazine + amount }
        : result;
}
export function reward(bag, cost, items) {
    return Bag.exchange(bag, cost, items);
}

export function quest(bag, previous, progress, definition) {
    if (previous === 'done')
        return { ok: true, bag, stage: 'done', message: definition.after };
    if (!previous && Object.keys(definition.acceptItems).length) {
        const gift = Bag.exchange(bag, {}, definition.acceptItems);
        return gift.ok
            ? {
                  ok: true,
                  bag: gift.bag,
                  stage: 'active',
                  message: definition.accepted,
              }
            : gift;
    }
    const objective = definition.objective;
    const ready = objective.counter
        ? (progress[objective.counter] || 0) >= objective.count
        : Object.entries(objective.items || {}).every(
              ([id, n]) => Bag.count(bag, id) >= n,
          );
    if (!ready)
        return { ok: true, bag, stage: 'active', message: definition.progress };
    const result = Bag.exchange(bag, objective.items || {}, definition.rewards);
    return result.ok
        ? {
              ok: true,
              bag: result.bag,
              stage: 'done',
              message: definition.completed,
          }
        : result;
}

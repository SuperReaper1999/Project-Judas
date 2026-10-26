import * as Bag from './inventory.js';
export function service(bag, state, definition, kind) {
    if (
        !Number.isFinite(state.fuel) ||
        state.fuel < 0 ||
        state.fuel > definition.tank ||
        !Number.isFinite(state.health) ||
        state.health < 0 ||
        state.health > 100
    )
        return { ok: false, reason: 'Invalid vehicle service state.' };
    if (kind === 'fuel') {
        if (definition.tank - state.fuel < 5 - 1e-9)
            return {
                ok: false,
                reason: 'Leave at least 5 L of tank space for a sealed fuel can.',
            };
        const r = Bag.remove(bag, 'fuel_can', 1);
        return r.ok
            ? {
                  ok: true,
                  bag: r.bag,
                  state: { ...state, fuel: state.fuel + 5 },
                  message: 'Added 5 L of fuel.',
              }
            : r;
    }
    if (kind !== 'repair' || state.health >= 100)
        return { ok: false, reason: 'No repair is needed.' };
    const r = Bag.remove(bag, 'repair_kit', 1);
    return r.ok
        ? {
              ok: true,
              bag: r.bag,
              state: { ...state, health: Math.min(100, state.health + 35) },
              message: 'Mechanical repair completed.',
          }
        : r;
}
export function burn(state, litres) {
    if (
        !Number.isFinite(litres) ||
        litres < 0 ||
        !Number.isFinite(state.fuel) ||
        state.fuel < 0
    )
        throw new Error('Invalid fuel consumption');
    const used = Math.min(state.fuel, litres);
    return {
        ...state,
        fuel: state.fuel - used,
        burned: (state.burned || 0) + used,
    };
}

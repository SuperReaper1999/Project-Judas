// A bounded, serializable resource ledger. Failed placements never debit it.
export const recipes = { floor: { wood: 4, stone: 0 }, wall: { wood: 4, stone: 1 }, fence: { wood: 3, stone: 0 }, pillar: { wood: 2, stone: 0 }, stonebase: { wood: 0, stone: 5 } };
export const kinds = Object.keys(recipes);
export function afford(state, kind) { const r = recipes[kind]; return state.inventory.wood >= r.wood && state.inventory.stone >= r.stone; }
export function debit(state, kind) { const r = recipes[kind]; state.inventory.wood -= r.wood; state.inventory.stone -= r.stone; state.stats.woodSpent += r.wood; state.stats.stoneSpent += r.stone; }
export function collect(state, kind, amount) { state.inventory[kind] += amount; state.stats[kind === 'wood' ? 'woodCollected' : 'stoneMined'] += amount; }

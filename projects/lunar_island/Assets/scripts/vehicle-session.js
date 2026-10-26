// Transient lookups only; each ordinary vehicle script owns its durable state.
export const vehicles = new Map();
export function vehicleFor(entity) {
    for (
        let depth = 0;
        entity?.valid && depth < 8;
        depth++, entity = entity.parent
    ) {
        const found = vehicles.get(entity.id);
        if (found) return found;
    }
    return null;
}
export const vehicleByReference = (ref) =>
    [...vehicles.values()].find((v) => v.reference === ref) || null;

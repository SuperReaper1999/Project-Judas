// Instance registry only. Durable state belongs to ordinary script instances.
export const actors = new Map();
export const corpses = new Map();
let controlled = null;
export const setPlayer = (p) => {
    controlled = p;
};
export const getPlayer = () => controlled;
export const clearPlayer = (p) => {
    if (controlled === p) controlled = null;
};
export const actorFor = (entity) =>
    entity?.valid ? actors.get(entity.id) || corpses.get(entity.id) : null;

// Transient registrations are deliberately NOT saved. Each instance re-registers
// only after its start/restore callback runs in the current world.
export const crates = new Map();
export const vehicles = new Map();
export const players = new Map();

import { CONFIG } from './definitions.js';
export const clamp = (x, lo = 0, hi = 100) => Math.max(lo, Math.min(hi, x));
export function initialVitals() {
    return {
        health: CONFIG.startingHealth,
        hunger: CONFIG.startingHunger,
        thirst: CONFIG.startingThirst,
        stamina: 100,
        breath: CONFIG.breathSeconds,
        injury: 0,
        elapsed: 0,
    };
}
export function tick(
    v,
    dt,
    { sprinting = false, underwater = false, swimming = false } = {},
) {
    v.elapsed += dt;
    v.hunger = clamp(v.hunger - CONFIG.hungerPerSecond * dt);
    v.thirst = clamp(v.thirst - CONFIG.thirstPerSecond * dt);
    v.stamina = clamp(
        v.stamina +
            (sprinting
                ? -CONFIG.staminaSprintPerSecond
                : swimming
                  ? -5
                  : CONFIG.staminaRecoveryPerSecond) *
                dt,
    );
    v.breath = clamp(
        v.breath + (underwater ? -dt : dt * 3),
        0,
        CONFIG.breathSeconds,
    );
    let damage =
        (v.hunger === 0 ? CONFIG.starvationDamagePerSecond : 0) +
        (v.thirst === 0 ? CONFIG.dehydrationDamagePerSecond : 0) +
        (v.breath === 0 ? 6 : 0);
    v.health = clamp(v.health - damage * dt);
}
export function consume(v, effects) {
    const result = { ...v };
    for (const [key, n] of Object.entries(effects))
        result[key] = clamp(result[key] + n, 0, key === 'injury' ? 5 : 100);
    return result;
}
export function fall(v, speed) {
    if (speed > 9) {
        v.health = clamp(v.health - (speed - 9) * 4);
        v.injury = Math.min(5, v.injury + 1);
        return true;
    }
    return false;
}

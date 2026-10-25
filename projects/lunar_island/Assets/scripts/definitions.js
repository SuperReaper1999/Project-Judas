// Generated from Authoring JSON. Rebuild with --data-only.
export const ITEMS = {
    wood: {
        name: 'Timber',
        stack: 30,
        weight: 0.6,
        category: 'material',
    },
    stone: {
        name: 'Stone',
        stack: 30,
        weight: 0.5,
        category: 'material',
    },
    fiber: {
        name: 'Plant fibre',
        stack: 40,
        weight: 0.04,
        category: 'material',
    },
    iron_ore: {
        name: 'Iron ore',
        stack: 20,
        weight: 0.8,
        category: 'material',
    },
    scrap: {
        name: 'Metal scrap',
        stack: 20,
        weight: 0.3,
        category: 'material',
    },
    berries: {
        name: 'Wild berries',
        stack: 12,
        weight: 0.1,
        category: 'food',
        use: {
            hunger: 5,
            thirst: 2,
        },
    },
    rations: {
        name: 'Trail rations',
        stack: 6,
        weight: 0.3,
        category: 'food',
        use: {
            hunger: 24,
            thirst: -3,
        },
    },
    cooked_meal: {
        name: 'Warm berry porridge',
        stack: 6,
        weight: 0.35,
        category: 'food',
        use: {
            hunger: 32,
            health: 5,
        },
    },
    empty_flask: {
        name: 'Empty water flask',
        stack: 1,
        weight: 0.2,
        category: 'supply',
    },
    water: {
        name: 'Full water flask',
        stack: 1,
        weight: 0.7,
        category: 'supply',
        use: {
            thirst: 40,
        },
        returns: 'empty_flask',
    },
    bandage: {
        name: 'Clean field dressing',
        stack: 8,
        weight: 0.1,
        category: 'medical',
        use: {
            health: 25,
            injury: -1,
        },
    },
    stone_axe: {
        name: 'Stone axe',
        stack: 1,
        weight: 1.2,
        category: 'tool',
        slot: 'hand',
        tool: 'axe',
    },
    stone_pickaxe: {
        name: 'Stone pickaxe',
        stack: 1,
        weight: 1.8,
        category: 'tool',
        slot: 'hand',
        tool: 'pickaxe',
    },
    work_coat: {
        name: 'Canvas work coat',
        stack: 1,
        weight: 0.8,
        category: 'clothing',
        slot: 'body',
    },
};
export const RECIPES = [
    {
        id: 'stone_axe',
        name: 'Stone axe',
        ingredients: {
            wood: 3,
            stone: 2,
            fiber: 2,
        },
        outputs: {
            stone_axe: 1,
        },
        station: 'hand',
        description: 'Fell trees, then process the fallen trunk into timber.',
    },
    {
        id: 'stone_pickaxe',
        name: 'Stone pickaxe',
        ingredients: {
            wood: 4,
            stone: 3,
            fiber: 2,
        },
        outputs: {
            stone_pickaxe: 1,
        },
        station: 'hand',
        description: "Extract stone and iron from the island's deposits.",
    },
    {
        id: 'bandage',
        name: 'Field dressing',
        ingredients: {
            fiber: 3,
        },
        outputs: {
            bandage: 1,
        },
        station: 'hand',
        description: 'Heal wounds and treat one fall injury.',
    },
    {
        id: 'cooked_meal',
        name: 'Berry porridge',
        ingredients: {
            berries: 5,
            wood: 1,
        },
        outputs: {
            cooked_meal: 1,
        },
        station: 'campfire',
        description: 'Prepare a warming meal at the homestead fire.',
    },
    {
        id: 'fiber',
        name: 'Salvage lashings',
        ingredients: {
            scrap: 1,
        },
        outputs: {
            fiber: 2,
        },
        station: 'hand',
        description: 'Strip salvage into useful binding material.',
    },
];
export const CONFIG = {
    slots: 18,
    maxWeight: 32,
    walkSpeed: 4.5,
    sprintSpeed: 7,
    jumpSpeed: 5.3,
    hungerPerSecond: 0.018,
    thirstPerSecond: 0.035,
    staminaSprintPerSecond: 18,
    staminaRecoveryPerSecond: 14,
    starvationDamagePerSecond: 1.2,
    dehydrationDamagePerSecond: 2,
    breathSeconds: 22,
    startingHealth: 100,
    startingHunger: 75,
    startingThirst: 65,
};

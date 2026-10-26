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
    planks: {
        name: 'Sawn planks',
        stack: 30,
        weight: 0.25,
        category: 'material',
    },
    nails: {
        name: 'Handmade nails',
        stack: 50,
        weight: 0.02,
        category: 'material',
    },
    metal_sheet: {
        name: 'Metal sheet',
        stack: 10,
        weight: 1,
        category: 'material',
    },
    tokens: {
        name: 'Trade tokens',
        stack: 9999,
        weight: 0,
        category: 'currency',
    },
    pistol: {
        name: 'Trail pistol',
        stack: 1,
        weight: 1.1,
        category: 'weapon',
        slot: 'hand',
    },
    shotgun: {
        name: 'Coach shotgun',
        stack: 1,
        weight: 2.8,
        category: 'weapon',
        slot: 'hand',
    },
    rifle: {
        name: 'Scavenger rifle',
        stack: 1,
        weight: 3.1,
        category: 'weapon',
        slot: 'hand',
    },
    knife: {
        name: 'Field knife',
        stack: 1,
        weight: 0.4,
        category: 'weapon',
        slot: 'hand',
    },
    bat: {
        name: 'Hardwood bat',
        stack: 1,
        weight: 1.2,
        category: 'weapon',
        slot: 'hand',
    },
    ammo_pistol: {
        name: 'Pistol rounds',
        stack: 60,
        weight: 0.012,
        category: 'ammunition',
    },
    ammo_shell: {
        name: 'Shotgun shells',
        stack: 24,
        weight: 0.035,
        category: 'ammunition',
    },
    ammo_rifle: {
        name: 'Rifle rounds',
        stack: 60,
        weight: 0.018,
        category: 'ammunition',
    },
    raw_meat: {
        name: 'Raw game meat',
        stack: 8,
        weight: 0.3,
        category: 'food',
        use: {
            hunger: 5,
            health: -4,
        },
    },
    hide: {
        name: 'Wolf hide',
        stack: 8,
        weight: 0.45,
        category: 'material',
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
    {
        id: 'planks',
        name: 'Saw planks',
        ingredients: {
            wood: 1,
        },
        outputs: {
            planks: 4,
        },
        station: 'hand',
        description: 'Boards for barricading openings.',
    },
    {
        id: 'nails',
        name: 'Forge nails',
        ingredients: {
            scrap: 1,
        },
        outputs: {
            nails: 6,
        },
        station: 'workbench',
        description: 'Work scrap into reusable fixings.',
    },
    {
        id: 'metal_sheet',
        name: 'Flatten metal sheet',
        ingredients: {
            scrap: 3,
        },
        outputs: {
            metal_sheet: 1,
        },
        station: 'workbench',
        description: 'Reinforce a door or window barricade.',
    },
    {
        id: 'roast_game',
        name: 'Roast game meat',
        station: 'campfire',
        ingredients: {
            raw_meat: 1,
            wood: 1,
        },
        outputs: {
            rations: 1,
        },
    },
    {
        id: 'field_knife',
        name: 'Make a field knife',
        station: 'workbench',
        ingredients: {
            scrap: 3,
            wood: 1,
        },
        outputs: {
            knife: 1,
        },
    },
    {
        id: 'hardwood_bat',
        name: 'Make a hardwood bat',
        station: 'hand',
        ingredients: {
            wood: 3,
        },
        outputs: {
            bat: 1,
        },
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

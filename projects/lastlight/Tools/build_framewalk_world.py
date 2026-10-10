"""Ordinary blockout buildings and inventory pickups for Lastlight.

The Framewalk route is content: floors, walls, ceilings and roofs use the same
static box bodies as the eight houses. Gravity selection belongs to the player's
boots script. No building name, tag or region changes the world's gravity.
"""
from pathlib import Path
import json
import math


PROJECT = Path(__file__).resolve().parents[1]
INVENTORY_TAGS = 18  # Existing physical bit plus the inventory_item bit.
BUILDINGS = [
    ('Framewalk workshop', 37, 8, 5, 6, 7.4, (.32, .46, .51)),
    ('Framewalk tower', 37, 27, 5, 6, 10.0, (.48, .43, .34)),
    ('Framewalk depot', 37, 45, 5, 5, 8.5, (.43, .38, .48)),
]


def transform(position=(0, 0, 0), yaw=0):
    return ('  position ' + ' '.join(map(str, position)) + '\n'
            f'  rotation {math.cos(yaw/2)} 0 {math.sin(yaw/2)} 0\n'
            '  scale 1 1 1\n')


def render(half, color):
    return ('  render box\n  render.half-extents ' + ' '.join(map(str, half)) + '\n'
            '  render.radius .5\n  render.color ' + ' '.join(map(str, color)) + '\n'
            '  render.alpha 1\n  render.secondary-color .8 .8 .8\n'
            '  render.secondary-alpha 1\n  render.mesh-asset ""\n'
            '  render.texture-asset ""\n')


def body(half, kind='static', mass=20):
    return (f'  body {kind} box\n  body.half-extents ' + ' '.join(map(str, half)) + '\n'
            f'  body.radius .5\n  body.terrain ""\n  body.mass {mass}\n'
            '  body.friction .7\n  body.restitution .05\n'
            '  body.initial-velocity 0 0 0\n  body.pickable false\n'
            '  body.managed false\n  body.compound-count 0\n'
            '  body.fluid-cavity-count 0\n')


def script(asset, properties):
    return ('  scripts 1\n  script.0.id 1\n'
            f'  script.0.asset "{asset}"\n  script.0.enabled true\n'
            '  script.0.properties ' + json.dumps(json.dumps(properties, separators=(',', ':'))) + '\n')


def entity(entity_id, name, fields):
    return f'object {entity_id} "{name}"\n{fields}end\n'


def pickup_entities(entity_id, item, position, ids, quantity=1):
    """The same body/script assembly is used for scene items and dropped items."""
    boots = item == 'framewalk_boots'
    half = (.4, .22, .32) if boots else (.42, .25, .32)
    fields = transform(position) + body(half, 'dynamic', 2 if boots else 3)
    fields += script(ids['items'], {'item': item, 'quantity': quantity,
                                  'prefab': ids[item + '_pickup']})
    fields += f'  tags {INVENTORY_TAGS}\n'
    output = entity(entity_id, 'Framewalk boots pickup' if boots else item.title() + ' supply pickup', fields)
    next_id = entity_id + 1

    def visual(name, pos, size, color):
        nonlocal output, next_id
        output += entity(next_id, name, transform(pos) + render(size, color) + f'  parent {entity_id}\n')
        next_id += 1

    if boots:
        for side in [-1, 1]:
            x = side * .19
            visual('Boot sole', (x, -.16, -.01), (.16, .055, .30), (.10, .15, .19))
            visual('Boot upper', (x, -.055, -.11), (.14, .065, .19), (.26, .34, .37))
            visual('Boot ankle', (x, .045, .09), (.135, .15, .10), (.26, .34, .37))
            visual('Boot amber clamp', (x, .10, .202), (.14, .045, .02), (.95, .61, .12))
    else:
        visual('Supply case', (0, -.015, 0), (.4, .22, .30), (.27, .34, .31))
        visual('Supply case bright lid', (0, .205, 0), (.41, .025, .31),
               (.92, .58, .12) if item == 'rockets' else (.30, .72, .67))
        visual('Supply case stripe', (0, .08, -.305), (.20, .065, .015), (.92, .88, .68))
    return output


def author_framewalk_assets(ids, track, header):
    """Create reusable pickups and a physical wooden door/debris assembly."""
    for item, quantity in [('framewalk_boots', 1), ('rockets', 2), ('grenades', 2)]:
        path = PROJECT / 'Assets/prefabs' / (item + '_pickup.judasprefab')
        # Register first because each pickup publishes its own prefab for drop.
        track(path, 'prefab')
        path.write_text(header + pickup_entities(1, item, (0, 0, 0), ids, quantity))

    debris = header + entity(1, 'Wooden entry debris lifetime',
                             transform() + script(ids['destructible'], {'cleanup': True, 'lifetime': 12}))
    # Six separate planks retain the intact 24 kg door mass. The existing
    # destructible script inherits real point/angular velocity and expires them.
    for i in range(6):
        half = (.175, 1.22, .085)
        debris += entity(i + 2, 'Broken entry plank ' + str(i + 1),
                         transform((-.875 + i * .35, 0, 0)) + render(half, (.48, .30, .15))
                         + body(half, 'dynamic', 4) + '  tags 2\n  parent 1\n')
    path = PROJECT / 'Assets/prefabs/framewalk_door_debris.judasprefab'
    path.write_text(debris)
    track(path, 'prefab')

    path = PROJECT / 'Assets/prefabs/framewalk_door.judasprefab'
    half = (1.05, 1.22, .085)
    path.write_text(header + entity(1, 'Breakable wooden entry', transform() + render(half, (.48, .30, .15))
                                   + body(half, mass=24) + script(ids['destructible'],
                                   {'health': 100, 'debris': ids['framewalk_door_debris']}) + '  tags 6\n'))
    track(path, 'prefab')


def author_framewalk_world(obj, ids):
    """Append geometry in a dedicated ID range; keep old town IDs stable."""
    next_id = 900

    def box(name, pos, half, color, physical=True):
        nonlocal next_id
        obj(next_id, name, transform(pos) + render(half, color) + (body(half) if physical else ''))
        next_id += 1

    def door(name, pos):
        nonlocal next_id
        half = (1.05, 1.22, .085)
        obj(next_id, name + ' breakable wooden entry', transform(pos) + render(half, (.48, .30, .15))
            + body(half, mass=24) + script(ids['destructible'],
                                  {'health': 100, 'debris': ids['framewalk_door_debris']}) + '  tags 6\n')
        next_id += 1

    for index, (name, x, z, hx, hz, roof, color) in enumerate(BUILDINGS):
        wall_top = roof - .24
        wall_half = wall_top / 2
        floor_top = .12
        opening_top = 2.8
        door_half = 1.15
        box(name + ' floor', (x, .06, z), (hx - .14, .06, hz - .14), (.45, .46, .43))

        # Both street/yard doorways are 2.3m wide and 2.68m high above the floor.
        # The tower is never represented by one solid building-sized body.
        for edge in [-1, 1]:
            for lo, hi in [(-hz, -door_half), (door_half, hz)]:
                box(name + (' street wall' if edge < 0 else ' yard wall'),
                    (x + edge * hx, wall_half, z + (lo + hi) / 2),
                    (.14, wall_half, (hi - lo) / 2), color)
            box(name + ' open entry lintel', (x + edge * hx, (opening_top + wall_top) / 2, z),
                (.14, (wall_top - opening_top) / 2, door_half), color)
            box(name + ' entry amber header', (x + edge * (hx + .15), 2.75, z),
                (.018, .06, 1.25), (.91, .61, .17), False)

        # The south side offers a separate real destructible door. The north
        # wall has an empty upper window, allowing a route onto the next canopy.
        side_z = z - hz
        for lo, hi in [(-hx, -door_half), (door_half, hx)]:
            box(name + ' south wall', (x + (lo + hi) / 2, wall_half, side_z),
                ((hi - lo) / 2, wall_half, .14), color)
        box(name + ' breakable entry lintel', (x, (opening_top + wall_top) / 2, side_z),
            (door_half, (wall_top - opening_top) / 2, .14), color)
        door(name, (x, floor_top + 1.22, side_z))
        for lo, hi in [(-hx, -1.5), (1.5, hx)]:
            box(name + ' north wall', (x + (lo + hi) / 2, wall_half, z + hz),
                ((hi - lo) / 2, wall_half, .14), color)
        box(name + ' north window lower wall', (x, 1.45, z + hz), (1.5, 1.45, .14), color)
        box(name + ' north window upper wall', (x, (5.7 + wall_top) / 2, z + hz),
            (1.5, (wall_top - 5.7) / 2, .14), color)

        # Four ordinary slabs leave a real 2.6 x 3.6m roof opening. Flat slab
        # undersides are ceilings; slab tops are roofs. A clear inner column
        # supplies another vertical face all the way from the floor to ceiling.
        for lo, hi in [(-hx - .18, -1.3), (1.3, hx + .18)]:
            box(name + ' ceiling and roof side', (x + (lo + hi) / 2, roof - .12, z),
                ((hi - lo) / 2, .12, hz + .18), (.22, .29, .33))
        for lo, hi in [(-hz - .18, -1.8), (1.8, hz + .18)]:
            box(name + ' ceiling and roof end', (x, roof - .12, z + (lo + hi) / 2),
                (1.3, .12, (hi - lo) / 2), (.22, .29, .33))
        box(name + ' inner climbing column', (x - 2, wall_half, z), (.32, wall_half, .6), (.60, .58, .50))
        box(name + ' interior return wall', (x + 2.6, wall_half, z + 2.8),
            (1.35, wall_half, .14), (.61, .59, .50))
        # An upper gallery exposes another underside while leaving most of the
        # atrium empty; it cannot seal the entrances or the roof opening.
        gallery = 3.5 if index != 1 else 4.5
        box(name + ' upper gallery', (x + 3.35, gallery, z - 2.8),
            (1.45, .12, 2.9), (.46, .43, .37))
        box(name + ' low bench', (x + 3.5, .38, z + 4), (1.0, .26, .5), (.40, .30, .21))
        box(name + ' entry approach', (x - hx - 2, .013, z), (2, .007, 1.4), (.40, .47, .47), False)
        for height in [3.2, 6.2]:
            box(name + ' amber height band', (x - hx - .145, height, z),
                (.012, .045, hz - .1), (.83, .55, .19), False)

    # Canopies meet the flat north/south walls with no legs across z17/z37.
    # Their underside is reachable from the ground; walking on top gives a
    # landing beside the upper windows and the next building's vertical face.
    for name, lo, hi, height in [('Workshop to tower canopy', 14, 21, 3.4),
                                 ('Tower to depot canopy', 33, 40, 3.7)]:
        box(name, (37, height, (lo + hi) / 2), (2.0, .12, (hi - lo) / 2), (.30, .44, .45))
        box(name + ' amber edge', (34.98, height, (lo + hi) / 2),
            (.012, .045, (hi - lo) / 2), (.91, .61, .17), False)

    # A low plinth makes the two boots readable beside the unchanged start.
    box('Framewalk boots display plinth', (2.4, .09, 18), (.65, .09, .6), (.36, .42, .43))
    box('Framewalk route first marker', (25.2, .014, 17), (1.2, .008, .22), (.91, .61, .17), False)
    box('Framewalk route second marker', (29.2, .014, 17), (1.2, .008, .22), (.91, .61, .17), False)
    assert next_id < 1400, 'Framewalk geometry overlaps pickup IDs'

    # Add complete entity blocks directly, keeping a reusable prefab for drop.
    # Supplies on roofs reward actual traversal; they use the existing stocks.
    pickups = [(1500, 'framewalk_boots', (2.4, .42, 18), 1),
               (1520, 'grenades', (40, 7.65, 5), 2),
               (1530, 'rockets', (40, 10.25, 30), 2),
               (1540, 'grenades', (40, 8.75, 47), 2)]
    return [pickup_entities(entity_id, item, position, ids, quantity)
            for entity_id, item, position, quantity in pickups]

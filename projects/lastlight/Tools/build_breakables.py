"""Ordinary Lastlight destructible prefab authoring; no engine changes.

Imported by build_town.py after script asset metadata has been registered.
Crates keep the original starter-pack mesh; their replacement pieces are modest
wood panels with real box colliders, equal total body mass and finite lifetime.
"""
from pathlib import Path
import json
import math

PROJECT = Path(__file__).resolve().parents[1]


def transform(position=(0, 0, 0), rotation=(1, 0, 0, 0)):
    return ('  position ' + ' '.join(map(str, position)) + '\n'
            '  rotation ' + ' '.join(map(str, rotation)) + '\n'
            '  scale 1 1 1\n')


def body(half, mass):
    return (f'  body dynamic box\n  body.half-extents {" ".join(map(str, half))}\n'
            f'  body.radius .5\n  body.terrain ""\n  body.mass {mass}\n'
            '  body.friction .65\n  body.restitution .05\n'
            '  body.initial-velocity 0 0 0\n  body.pickable false\n'
            '  body.managed false\n  body.compound-count 0\n  body.fluid-cavity-count 0\n')


def render(half=(.5, .5, .5), color=(.39, .22, .1), mesh=''):
    return (f'  render {"mesh" if mesh else "box"}\n'
            f'  render.half-extents {" ".join(map(str, half))}\n'
            f'  render.radius .5\n  render.color {" ".join(map(str, color))}\n'
            '  render.alpha 1\n  render.secondary-color .25 .14 .06\n'
            f'  render.secondary-alpha 1\n  render.mesh-asset "{mesh}"\n'
            '  render.texture-asset ""\n')


def script(asset, properties):
    return (f'  scripts 1\n  script.0.id 1\n  script.0.asset "{asset}"\n'
            '  script.0.enabled true\n'
            f'  script.0.properties {json.dumps(json.dumps(properties, separators=(",", ":")))}\n')


def entity(entity_id, name, fields):
    return f'object {entity_id} "{name}"\n{fields}end\n'


def author_assets(ids, track, header):
    """Return crate/barrel/debris IDs; track(path, kind) uses the normal pack IDs."""
    script_id = ids['destructible']
    # Crate's six panels are non-overlapping and sum to the intact 18 kg body.
    crate_pieces = [
        ((-.46, 0, 0), (.04, .42, .42), (1, 0, 0, 0), 3),
        ((.46, 0, 0), (.04, .42, .42), (1, 0, 0, 0), 3),
        ((0, -.46, 0), (.5, .04, .5), (1, 0, 0, 0), 3),
        ((0, .46, 0), (.5, .04, .5), (1, 0, 0, 0), 3),
        ((0, 0, -.46), (.42, .42, .04), (1, 0, 0, 0), 3),
        ((0, 0, .46), (.42, .42, .04), (1, 0, 0, 0), 3),
    ]
    # Eight broad staves and two lids: a deliberately coarse breakup, not a
    # fine fracture simulation. All ten pieces have separate dynamic bodies.
    barrel_pieces = []
    for index in range(8):
        angle = index * math.tau / 8
        barrel_pieces.append(((math.sin(angle)*.30, 0, math.cos(angle)*.30),
                              (.115, .46, .035),
                              (math.cos(angle/2), 0, math.sin(angle/2), 0), 2.5))
    barrel_pieces += [((0, -.50, 0), (.25, .035, .25), (1, 0, 0, 0), 2),
                      ((0, .50, 0), (.25, .035, .25), (1, 0, 0, 0), 2)]
    result = {}
    for kind, pieces in [('crate', crate_pieces), ('barrel', barrel_pieces)]:
        debris = header + entity(1, f'{kind.title()} debris lifetime',
                                 transform() + script(script_id, {'cleanup': True, 'lifetime': 12}))
        for index, (position, half, rotation, mass) in enumerate(pieces, 2):
            # Inherit no gameplay tags; only ordinary physical response.
            debris += entity(index, f'{kind.title()} fragment {index-1}',
                             transform(position, rotation) + render(half) + body(half, mass)
                             + '  tags 2\n  parent 1\n')
        path = PROJECT/'Assets/prefabs'/f'{kind}_debris.judasprefab'
        path.write_text(debris)
        result[kind+'_debris'] = track(path, 'prefab')

    for kind, model, half, mass, health in [
            ('crate', 'wood_crate', (.5, .5, .5), 18, 60),
            ('barrel', 'barrel', (.37, .545, .37), 24, 90)]:
        intact = header + entity(1, f'Breakable {kind}', transform() + body(half, mass)
                                 + script(script_id, {'health': health, 'debris': result[kind+'_debris']})
                                 + '  tags 6\n')
        intact += entity(2, f'{kind.title()} starter-pack visual',
                         transform((0, -half[1], 0)) + render(color=(1, 1, 1), mesh=ids[model]) + '  parent 1\n')
        path = PROJECT/'Assets/prefabs'/f'breakable_{kind}.judasprefab'
        path.write_text(intact)
        result['breakable_'+kind] = track(path, 'prefab')
    return result


def scene_entity(entity_id, kind, position, ids, yaw=0):
    """Authored direct instance with the same settings as its reusable prefab.

    Builders can append these two ordinary scene entities without an authored
    prefab-instance encoding. The prefab assets remain available for JS spawn.
    """
    half, mass, health, model = ((.5, .5, .5), 18, 60, 'wood_crate') if kind == 'crate' else ((.37, .545, .37), 24, 90, 'barrel')
    rotation = (math.cos(yaw/2), 0, math.sin(yaw/2), 0)
    output = entity(entity_id, f'Breakable {kind}', transform(position, rotation) + body(half, mass)
                    + script(ids['destructible'], {'health': health, 'debris': ids[kind+'_debris']})
                    + '  tags 6\n')
    output += entity(entity_id+1, f'{kind.title()} starter-pack visual',
                     transform((0, -half[1], 0)) + render(color=(1, 1, 1), mesh=ids[model]) + f'  parent {entity_id}\n')
    return output

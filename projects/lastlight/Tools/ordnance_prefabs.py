"""Ordinary physical projectile content; explosive behaviour is project JS."""
from pathlib import Path
from build_breakables import transform, render, script, entity

PROJECT = Path(__file__).resolve().parents[1]


def projectile_body(radius, mass, restitution):
    return (f'  body dynamic sphere\n'
            f'  body.half-extents {radius} {radius} {radius}\n'
            f'  body.radius {radius}\n  body.terrain ""\n  body.mass {mass}\n'
            f'  body.friction .5\n  body.restitution {restitution}\n'
            '  body.initial-velocity 0 0 0\n  body.pickable false\n'
            '  body.managed false\n  body.compound-count 0\n'
            '  body.fluid-cavity-count 0\n')


def author_ordnance(ids, track, header):
    """Create and register rocket/grenade prefabs; return their stable asset IDs."""
    result = {}
    for kind, radius, mass, restitution, fuse, blast_radius, damage, offset in [
            ('rocket', .12, 1.8, .05, 4, 6.5, 130, (0, 0, 0)),
            ('grenade', .09, .45, .45, 2.4, 5.5, 100, (0, -.075, 0))]:
        properties = {'kind': kind, 'fuse': fuse, 'radius': blast_radius,
                      'damage': damage}
        content = header + entity(1, f'{kind.title()} projectile',
                                  transform() + projectile_body(radius, mass, restitution)
                                  + script(ids['explosive'], properties) + '  tags 2\n')
        content += entity(2, f'{kind.title()} visual',
                          transform(offset) + render(color=(1, 1, 1), mesh=ids[kind])
                          + '  parent 1\n')
        name = kind + '_projectile'
        path = PROJECT/'Assets/prefabs'/f'{name}.judasprefab'
        path.write_text(content)
        result[name] = track(path, 'prefab')
    return result

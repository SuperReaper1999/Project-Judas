#!/usr/bin/env python3
"""Generate this workshop's original editable source content, never build Judas.
Use --replace only to intentionally overwrite generated scenes/UI/source models.
"""
import argparse
import copy
import hashlib
import json
import math
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]


def asset(path):
    return hashlib.sha256(('JudasParticlesV2Workshop/' + path).encode()).hexdigest()[:32]


def meta(path, kind):
    (ROOT / (path + '.judasmeta')).write_text(
        f'JudasAssetMeta 1\nid "{asset(path)}"\ntype {kind}\nsource "Particle V2 Workshop source"\n')


def obj(i, name, position=(0, 0, 0), **fields):
    if 'render' in fields:
        fields = {'render.half-extents': [.5, .5, .5], 'render.radius': 1,
                  'render.color': [1, 1, 1], 'render.alpha': 1,
                  'render.secondary-color': [.5, .5, .5], 'render.secondary-alpha': 1,
                  'render.mesh-asset': '', 'render.texture-asset': '', **fields}
    if 'body' in fields:
        fields = {'body.half-extents': [.5, .5, .5], 'body.radius': 1,
                  'body.terrain': '', 'body.mass': 1, 'body.friction': .75,
                  'body.restitution': .1, 'body.initial-velocity': [0, 0, 0],
                  'body.pickable': False, 'body.managed': False,
                  'body.compound-count': 0, **fields}
    return {'id': i, 'name': name, 'fields': {
        'position': list(position), 'rotation': [1, 0, 0, 0], 'scale': [1, 1, 1], **fields}}


def box(i, name, position, half, color, motion='static', receiver=True):
    return obj(i, name, position, render={'symbol': 'box'},
               **{'render.half-extents': half, 'render.color': color,
                  'body': [{'symbol': motion}, {'symbol': 'box'}],
                  'body.half-extents': half, 'tags': (2 if receiver else 0)})


def emitter(i, name, position, *, stress=False, paint=False):
    return obj(i, name, position, tags=1, **{
        'particle-emitter': [], 'particle.enabled': True, 'particle.loop': not stress,
        'particle.local-space': False, 'particle.gravity': not stress,
        'particle.rate': 0 if stress else 35, 'particle.lifetime': 3600 if stress else 5,
        'particle.size': .045 if stress else .13, 'particle.end-size': .045 if stress else .1,
        'particle.burst': 100000 if stress else 0, 'particle.capacity': 100000 if stress else 160,
        'particle.spread': [14, 5, 10] if stress else [3, .1, 3],
        'particle.velocity': [0, -.15, 0] if stress else [0, -2, 0],
        'particle.variation': [.1, .1, .1] if stress else [.15, .15, .15],
        'particle.acceleration': [0, 0, 0], 'particle.texture': asset('Assets/textures/disc.png'),
        'particle.color': [.08, .65, 1] if paint else [1, .45, .08], 'particle.alpha': .85,
        'particle.end-color': [.1, .55, 1] if paint else [1, .15, .04], 'particle.end-alpha': .85,
        'particle.seed': 8500 + i, 'particle.backend': 'gpu' if stress else 'cpu',
        'particle.collision': True, 'particle.collision-radius': .025 if stress else .055,
        'particle.restitution': .65, 'particle.friction': .2,
        'particle.response': 'remove' if paint else 'bounce',
        'particle.impacts': 'reliable' if paint else 'off',
        'particle.impact-cell-size': .5, 'particle.impact-weight': .2,
        'particle.max-impact-cells': 512, 'particle.max-contacts': 8,
        'gravity-selection.mode': 'uniform', 'gravity-selection.source': 0,
        'gravity-selection.acceleration': [0, -9.81, 0]})


def model(i, name, source, position):
    return obj(i, name, position, tags=2, render={'symbol': 'mesh'}, **{
        'render.mesh-asset': asset(f'Assets/models/{source}.obj'), 'render.color': [1, 1, 1],
        'body': [{'symbol': 'static'}, {'symbol': 'triangle-mesh'}],
        'body.collision-asset': asset(f'Assets/collision/{source}.judascollision')})


def scene(kind):
    objects = [obj(1, 'Particles V2 game controller', scripts=1, **{
        'script.0.id': 1, 'script.0.asset': asset('Assets/scripts/lab.js'),
        'script.0.enabled': True, 'script.0.properties': json.dumps({'caseKey': kind}),
        'ui.asset': asset('Assets/ui/lab.judasui'), 'ui.name': 'particle_lab', 'ui.enabled': True})]
    if kind == 'stress':
        objects += [box(100, 'Stress floor', [0, -.3, 0], [18, .3, 13], [.25, .3, .35], receiver=False)]
        for i in range(12):
            objects.append(box(110 + i, 'Static GPU collider',
                               [(i % 4 - 1.5) * 6, 1, (i // 4 - 1) * 7],
                               [1.25, 1, 1.25], [.2, .4, .55], receiver=False))
        objects.append(emitter(10, 'Explicit 100,000 GPU visual particles', [0, 7, 0], stress=True))
    else:
        objects.append(box(100, 'Continuous physical paint receiver', [-6, -.15, 0],
                           [4, .15, 4], [1, 1, 1]))
        for z in range(8):
            for x in range(8):
                objects.append(obj(500 + z * 8 + x, 'Game-owned coating tile',
                                   [-9.5 + x, .006, z - 3.5], render={'symbol': 'box'},
                                   **{'render.half-extents': [.49, .005, .49], 'render.color': [1, 1, 1]}))
        objects += [box(200, 'Bounce tray floor', [6, -.2, 0], [3.8, .2, 4], [.17, .22, .3], receiver=False),
                    box(201, 'Native rotating paint receiver', [6, 1.8, 0], [2.5, .18, .45], [1, 1, 1], 'kinematic'),
                    emitter(10, 'Reliable paint droplets', [-6, 5, 0], paint=True),
                    emitter(11, 'One-way bounce particles', [6, 5, 0]),
                    model(300, 'Cooked undulating terrain', 'terrain', [-6, 0, -11]),
                    model(301, 'Two-sided cooked cave arch', 'cave', [6, 0, -11])]
        for i, x in enumerate([-6, 6]):
            e = emitter(12 + i, 'Reliable terrain paint' if i == 0 else 'Reliable cave paint', [x, 5, -11], paint=True)
            e['fields']['particle.rate'] = 12
            e['fields']['particle.capacity'] = 64
            e['fields']['particle.spread'] = [2, .1, 2]
            objects.append(e)
    if kind == 'rotated':
        # The complete authored geometry rotates; this is not a camera trick.
        s = math.sqrt(.5)
        for o in objects[1:]:
            p = o['fields']['position']
            o['fields']['position'] = [-p[1], p[0], p[2]]
            o['fields']['rotation'] = [s, 0, 0, s]
            if 'particle-emitter' in o['fields']:
                o['fields']['gravity-selection.acceleration'] = [9.81, 0, 0]
    return {'kind': 'scene', 'schema': 1, 'data': {'settings': {
        'name': 'Particles V2 / ' + kind, 'world-origin': [0, 0, 0],
        'sun-direction': [-.3, .85, .4], 'sun-color': [.8, .85, 1], 'ambient': [.24, .26, .3],
        'linear-rendering': True, 'next-id': 2000, 'fluid-scale': 1,
        'fidelity-policy': {'mode': {'symbol': 'none'}}}, 'objects': objects}}


def models():
    vertices, faces = [], []
    n = 17
    for z in range(n):
        for x in range(n):
            xx, zz = (x / (n - 1) - .5) * 8, (z / (n - 1) - .5) * 8
            vertices.append((xx, .6 * math.sin(xx * .7) * math.cos(zz * .5), zz))
    for z in range(n - 1):
        for x in range(n - 1):
            a = z * n + x + 1
            faces += [(a, a + n, a + 1), (a + 1, a + n, a + n + 1)]
    write_obj('terrain', vertices, faces)
    vertices = [(3 * math.cos(i * math.pi / 12), 3 * math.sin(i * math.pi / 12), z)
                for z in [-3, 3] for i in range(13)]
    faces = []
    for i in range(12):
        a = i + 1
        faces += [(a, a + 1, a + 13), (a + 1, a + 14, a + 13)]
    write_obj('cave', vertices, faces)


def write_obj(name, vertices, faces):
    path = f'Assets/models/{name}.obj'
    text = ''.join('v %.8f %.8f %.8f\n' % point for point in vertices)
    text += ''.join('f %d %d %d\n' % face for face in faces)
    (ROOT / path).write_text(text)
    meta(path, 'mesh')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    if (ROOT / 'Scenes/workshop.judas').exists() and not args.replace:
        raise SystemExit('Generated content exists; choose --replace only to overwrite it.')
    for d in ['Assets/models', 'Assets/collision', 'Assets/scripts', 'Assets/fonts',
              'Assets/textures', 'Assets/ui', 'Scenes']:
        (ROOT / d).mkdir(parents=True, exist_ok=True)
    source = REPO / 'projects/renderer_v2_lab/Assets'
    for name in ['DejaVuSans.ttf', 'LICENSE.txt']:
        shutil.copyfile(source / 'fonts' / name, ROOT / 'Assets/fonts' / name)
    meta('Assets/fonts/DejaVuSans.ttf', 'font')
    shutil.copyfile(source / 'textures/disc.png', ROOT / 'Assets/textures/disc.png')
    meta('Assets/textures/disc.png', 'texture')
    shutil.copyfile(source / 'scripts/math.js', ROOT / 'Assets/scripts/math.js')
    meta('Assets/scripts/math.js', 'script')
    meta('Assets/scripts/lab.js', 'script')
    models()
    commands = []
    for name in ['terrain', 'cave']:
        output = f'Assets/collision/{name}.judascollision'
        meta(output, 'collision')
        commands.append(['judas_collision_cook', 'mesh', f'Assets/models/{name}.obj',
                         output, asset(f'Assets/models/{name}.obj'), '0', '--two-sided'])
    (ROOT / 'Tools/COOK_COMMANDS.json').write_text(json.dumps(commands, indent=2) + '\n')
    project = json.loads((REPO / 'projects/renderer_v2_lab/renderer_v2_lab.judasproj').read_text())
    p = project['data']
    p.update(name='Judas Particles V2 / Surface Workshop', startupScene='Scenes/workshop.judas',
             saveIdentity=hashlib.sha256(b'JudasParticlesV2Workshop-v1').hexdigest(),
             exportScenes=['Scenes/workshop.judas', 'Scenes/stress.judas', 'Scenes/rotated.judas'],
             runtimeAssets=[asset('Assets/scripts/lab.js'), asset('Assets/scripts/math.js'), asset('Assets/ui/lab.judasui')])
    p['classification']['tags'] = {'nextId': 2, 'entries': [{'id': 0, 'name': 'emitter'}, {'id': 1, 'name': 'receiver'}]}
    mapping = {'save': 'F6', 'load': 'F7', 'reset': 'R', 'pause': 'Escape', 'watch': 'T',
               'case_1': '1', 'case_2': '2', 'case_3': '3', 'emission': 'P', 'burst': 'B', 'clean': 'C'}
    p['input'] = [v for v in p['input'] if v['name'] in ['look_x', 'look_y', 'move_x', 'move_y', 'move_z']]
    for name, key in mapping.items():
        p['input'].append({'name': name, 'axis': False, 'bindings': [{'control': 'key:' + key, 'scale': 1, 'deadzone': 0}]})
    (ROOT / 'particle_v2_lab.judasproj').write_text(json.dumps(project, indent=2) + '\n')
    for kind in ['workshop', 'stress', 'rotated']:
        (ROOT / 'Scenes' / (kind + '.judas')).write_text(json.dumps(scene(kind), indent=2) + '\n')
    ui = json.loads((source / 'ui/rendering.judasui').read_text())
    ui['data']['elements'] = [e for e in ui['data']['elements'] if not e['id'].startswith('case_') or e['id'] in ['case_1', 'case_2', 'case_3']]
    for e in ui['data']['elements']:
        if e.get('font'):
            e['font'] = asset('Assets/fonts/DejaVuSans.ttf')
        if e['id'] == 'title':
            e['text'] = 'PARTICLES V2 / SURFACE WORKSHOP'
        if e['id'] == 'help':
            e['text'] = '1 Workshop | 2 100k visual stress | 3 Rotated gravity | T watch/free camera\nP pause particles | B burst | C clean coating | F6 save | F7 load | Esc menu'
        if e['id'] == 'status':
            e['text'] = 'Actual collisions paint receiving materials. Choose the 100k stress case explicitly.'
        if e['id'].startswith('case_'):
            n = int(e['id'][-1])
            e['text'] = ['1 Surface workshop', '2 100k GPU stress', '3 Rotated gravity'][n - 1]
            e['size'] = [220, 37]
            e['offset'] = [24 + (n - 1) * 230, 62]
    (ROOT / 'Assets/ui/lab.judasui').write_text(json.dumps(ui, indent=2) + '\n')
    meta('Assets/ui/lab.judasui', 'ui')
    print(ROOT)
    print('Source generated. Cook the two collision assets using Tools/COOK_COMMANDS.json before validation.')


if __name__ == '__main__':
    main()

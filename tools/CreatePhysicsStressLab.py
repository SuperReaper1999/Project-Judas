#!/usr/bin/env python3
"""Author the ordinary M87 lab. This is content generation, not engine dispatch.

All cabinets, paddles, shelves and loose pieces below are ordinary rigid bodies.
The controller is deliberately shared by the room rather than every physical item.
Run from any directory; existing hand-written project scripts are preserved.
"""
from pathlib import Path
import copy
import hashlib
import json
import math
import shutil

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'projects/physics_stress_lab'
SOURCE = ROOT / 'projects/weather_lab'


def identity(path):
    return hashlib.md5(('judas-m87-stress/' + str(path)).encode()).hexdigest()


def write(path, data):
    destination = OUT / path
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(data if isinstance(data, str) else json.dumps(data, indent=2) + '\n')


def metadata(path, kind):
    write(str(path) + '.judasmeta', 'JudasAssetMeta 1\nid "' + identity(path) + '"\ntype ' + kind + '\nsource "M87 original editable Physics Stress Lab"\n')


def asset(path, kind, data):
    write(path, data)
    metadata(path, kind)
    return identity(path)


def obj(number, name, position=(0, 0, 0), **fields):
    return {'id': number, 'name': name, 'fields': {'position': list(position), 'rotation': [1, 0, 0, 0], 'scale': [1, 1, 1], **fields}}


def box(number, name, position, half, color, dynamic=False, mass=8, tags=0, sphere=False, decorative=False):
    fields = {'render': {'symbol': 'sphere' if sphere else 'box'}, 'render.half-extents': half,
              'render.radius': half[0], 'render.color': color, 'render.alpha': 1,
              'render.secondary-color': [.7, .7, .7], 'render.secondary-alpha': 1,
              'render.mesh-asset': '', 'render.texture-asset': '', 'tags': tags}
    if not decorative:
        fields.update({'body': {'motion': {'symbol': 'dynamic' if dynamic else 'static'}, 'shape': {'symbol': 'sphere' if sphere else 'box'}},
                       'body.half-extents': half, 'body.radius': half[0], 'body.terrain': '',
                       'body.mass': mass, 'body.friction': .65, 'body.restitution': .025,
                       'body.initial-velocity': [0, 0, 0], 'body.pickable': False,
                       'body.managed': False, 'body.compound-count': 0, 'body.fluid-cavity-count': 0})
    return obj(number, name, position, **fields)


SETTINGS = copy.deepcopy(json.loads((SOURCE / 'Scenes/flat.judas').read_text())['data']['settings'])
SETTINGS.update({'name': 'Physics Stress Lab', 'world-origin': [0, 0, 0], 'next-id': 20000,
                 'sun-direction': [-.4, .8, .5], 'ambient': [.29, .32, .39]})


def scene(objects, name):
    settings = copy.deepcopy(SETTINGS)
    settings['name'] = name
    return {'kind': 'scene', 'schema': 1, 'data': {'settings': settings, 'objects': objects}}


def joint(number, name, body_a, body_b, anchor_a, anchor_b, kind, frame=(1, 0, 0, 0), lower=-.28, upper=.28, stiffness=35, damping=8):
    return obj(number, name, **{'joint': kind, 'joint.body-a': body_a, 'joint.body-b': body_b,
        'joint.anchor-a': anchor_a, 'joint.anchor-b': anchor_b, 'joint.frame-a': list(frame), 'joint.frame-b': list(frame),
        'joint.enabled': True, 'joint.limits': True, 'joint.lower': lower, 'joint.upper': upper,
        'joint.motor': False, 'joint.speed': 0, 'joint.max-force': 100,
        'joint.spring': True, 'joint.rest': 0, 'joint.stiffness': stiffness, 'joint.damping': damping,
        'joint.rotational-resistance': .25})


font_path = 'Assets/fonts/DejaVuSans.ttf'
(OUT / 'Assets/fonts').mkdir(parents=True, exist_ok=True)
shutil.copy2(SOURCE / font_path, OUT / font_path)
shutil.copy2(SOURCE / 'Assets/fonts/LICENSE.txt', OUT / 'Assets/fonts/LICENSE.txt')
metadata(font_path, 'font')
(OUT / 'Assets/scripts').mkdir(parents=True, exist_ok=True)
shutil.copy2(SOURCE / 'Assets/scripts/math.js', OUT / 'Assets/scripts/math.js')
metadata('Assets/scripts/math.js', 'script')
metadata('Assets/scripts/lab.js', 'script')

ui = json.loads((SOURCE / 'Assets/ui/weather.judasui').read_text())
for element in ui['data']['elements']:
    if element.get('font'):
        element['font'] = identity(font_path)
    if element['id'] == 'title':
        element.update(text='JUDAS / THE MECHANICAL ARCADE', color=[.94, .78, .37, 1], fontSize=24)
    elif element['id'] == 'help':
        element.update(text='1 / 2 / 3: 128 / 512 / 1024 LOOSE BODIES   |   4: ROTATED ROOM   |   M: MOTOR POWER\nF5: WATCH TOUR   |   J: IMPULSE ONE ASSEMBLY   |   K: IMPULSE ALL   |   P: DROP 32   |   ESC: PAUSE', fontSize=14)
    elif element['id'] == 'status':
        element.update(text='Preparing ordinary bodies and joints...', size=[1010, 110], background=[.025, .035, .055, .86], color=[.91, .95, 1, 1])
    elif element['id'] == 'reload':
        element['text'] = 'RELOAD PHYSICS LAB'
ui_id = asset('Assets/ui/stress.judasui', 'ui', ui)

prefabs = []
for name, sphere in [('box', False), ('sphere', True)]:
    data = scene([box(1, 'Ordinary runtime stress ' + name, [0, 0, 0], [.3, .3, .3], [.94, .56, .23] if not sphere else [.22, .73, .78], True, 2, 9, sphere)], 'Physics stress piece')
    data['kind'] = 'prefab'
    prefabs.append(asset('Assets/prefabs/' + name + '.judasprefab', 'prefab', data))
controller_path = OUT / 'Assets/scripts/lab.js'
controller = controller_path.read_text()
controller = controller.replace('PLACEHOLDER_BOX', prefabs[0]).replace('PLACEHOLDER_SPHERE', prefabs[1])
controller_path.write_text(controller)


def room(count, tilted=False):
    objects = [obj(10, 'One public JudasJS room controller', **{'scripts': 1, 'script.0.id': 1,
        'script.0.asset': identity('Assets/scripts/lab.js'), 'script.0.enabled': True,
        'script.0.properties': json.dumps({'count': count, 'tilted': tilted}),
        'ui.asset': ui_id, 'ui.name': 'physics_stress_lab', 'ui.enabled': True}),
        obj(2, 'Ordinary uniform gravity', **{'gravity': {'kind': {'symbol': 'uniform'}, 'magnitude': 9.81}, 'gravity.region': {'shape': {'symbol': 'sphere'}, 'radius': 100000}}),
        box(3, 'Solid arcade floor', (0, -.25, 0), [42, .25, 42], [.13, .16, .21])]
    # A real frame, a driven translating shelf, two spring-hinged paddles and
    # one loose ball per cabinet. The three articulated bodies share joints;
    # the ball participates only through actual contact with the mechanism.
    colors = [[.22, .55, .64], [.55, .29, .62], [.69, .4, .24], [.27, .6, .42]]
    for index in range(32):
        base = 1000 + index * 20
        x, z = (index % 8 - 3.5) * 7.4, -28 + (index // 8) * 6.3
        color = colors[index // 8]
        objects += [box(base, 'Cabinet rear / physical wall', (x, 1.14, z - 1.27), [1.35, 1.14, .12], color),
                    box(base + 1, 'Cabinet left / physical wall', (x - 1.35, 1.14, z), [.12, 1.14, 1.35], color),
                    box(base + 2, 'Cabinet right / physical wall', (x + 1.35, 1.14, z), [.12, 1.14, 1.35], color),
                    box(base + 3, 'Cabinet front / physical rim', (x, .8, z + 1.28), [1.35, .12, .12], color),
                    # .80 m half-length + .26 m stroke stays within the rear
                    # wall's 1.15 m inner clearance; no powered wall overlap.
                    box(base + 4, 'Articulated translating shelf', (x, .95, z), [1.12, .12, .80], [.75, .63, .33], True, 8, 5),
                    box(base + 5, 'Left spring paddle', (x - .55, 1.22, z + .38), [.48, .075, .16], [.93, .73, .3], True, 1.2, 1),
                    box(base + 6, 'Right spring paddle', (x + .55, 1.22, z + .38), [.48, .075, .16], [.93, .73, .3], True, 1.2, 1),
                    box(base + 7, 'Free contact-driven cabinet ball', (x, 1.41, z - .4), [.23, .23, .23], [.32, .79, .86], True, 1.5, 1, True)]
        drive = joint(base + 8, 'Finite-force arcade shelf drive', base + 4, 0, [0, 0, 0], [x, .95, z], 3,
                      (math.sqrt(.5), 0, -math.sqrt(.5), 0), lower=-.26, upper=.26, stiffness=22, damping=10)
        drive['fields']['tags'] = 2
        objects.append(drive)
        for side, offset in [(5, -.55), (6, .55)]:
            objects.append(joint(base + side + 4, 'Paddle hinge / physical spring', base + side, base + 4,
                [0, 0, 0], [offset, .27, .38], 1, (math.sqrt(.5), 0, 0, math.sqrt(.5)), lower=-.65, upper=.65, stiffness=8, damping=.8))
        # The sign is explicitly decorative; every contact surface above is solid.
        objects.append(box(base + 12, 'Cabinet colored header / decoration', (x, 2.38, z - 1.25), [1.35, .14, .14], [.9, .7, .26], decorative=True))
    # Segregated shallow pit: every loose object is active physics until the
    # ordinary engine sleeping criteria say otherwise; no scripted freezing.
    for number, p, half in [(40, [0, 1.1, 36], [26, 1.1, .2]), (41, [0, 1.1, 2], [26, 1.1, .2]),
                             (42, [-26, 1.1, 19], [.2, 1.1, 17]), (43, [26, 1.1, 19], [.2, 1.1, 17])]:
        objects.append(box(number, 'Loose-body pit / solid retaining wall', p, half, [.19, .25, .33]))
    for index in range(count):
        x = (index % 16 - 7.5) * .74
        z = 10 + ((index // 16) % 16) * .74
        y = .34 + (index // 256) * .68
        sphere = index % 3 == 0
        objects.append(box(5000 + index, 'Loose sphere' if sphere else 'Loose box', [x, y, z], [.3, .3, .3], [.23, .67, .76] if sphere else [.84, .47, .22], True, 2, 9, sphere))
    if tilted:
        # Rotate all physical authoring, anchors and gravity together, not a
        # scene-name physics branch. Joint world-side anchors are owner-local.
        angle = .68
        c, s = math.cos(angle), math.sin(angle)
        q = [math.cos(angle / 2), 0, 0, math.sin(angle / 2)]
        for item in objects:
            if item['id'] == 10:
                continue
            f = item['fields']
            x, y, z = f['position']
            f['position'] = [c*x-s*y, s*x+c*y, z]
            f['rotation'] = q[:]
    return scene(objects, 'Physics Stress Lab / ' + ('rotated room' if tilted else str(count) + ' loose bodies'))


for count in [128, 512, 1024]:
    write('Scenes/room' + str(count) + '.judas', room(count))
write('Scenes/rotated.judas', room(128, True))

project = copy.deepcopy(json.loads((SOURCE / 'weather_lab.judasproj').read_text()))
data = project['data']
data.update(name='Judas Physics Stress Lab', startupScene='Scenes/room128.judas', worldManifest='',
            saveIdentity=hashlib.sha256(b'Judas Physics Stress Lab M87').hexdigest(), runtimeAssets=prefabs,
            exportScenes=['Scenes/room128.judas', 'Scenes/room512.judas', 'Scenes/room1024.judas', 'Scenes/rotated.judas'])
data['classification']['tags'] = {'nextId': 4, 'entries': [{'id': i, 'name': name} for i, name in enumerate(['stress_dynamic', 'arcade_drive', 'arcade_chassis', 'loose_piece'])]}
bindings = [('count_128', '1'), ('count_512', '2'), ('count_1024', '3'), ('rotated', '4'),
            ('motors', 'M'), ('kick_one', 'J'), ('kick_all', 'K'), ('spawn', 'P'), ('watch', 'F5'),
            ('save', 'F6'), ('load', 'F7'), ('reset', 'R'), ('pause', 'Escape')]
data['input'] = [{'name': name, 'axis': False, 'bindings': [{'control': 'key:' + key, 'scale': 1., 'deadzone': 0.}]} for name, key in bindings]
for name, control in [('look_x', 'mouse:dx'), ('look_y', 'mouse:dy')]:
    data['input'].append({'name': name, 'axis': True, 'bindings': [{'control': control, 'scale': 1., 'deadzone': 0.}]})
for name, negative, positive in [('move_x', 'A', 'D'), ('move_z', 'S', 'W'), ('move_y', 'Q', 'E')]:
    data['input'].append({'name': name, 'axis': True, 'bindings': [{'control': 'key:' + negative, 'scale': -1., 'deadzone': 0.}, {'control': 'key:' + positive, 'scale': 1., 'deadzone': 0.}]})
for name, control in [('ui_click', 'mouse:Left'), ('ui_activate', 'key:Return'), ('ui_up', 'key:Up'), ('ui_down', 'key:Down')]:
    data['input'].append({'name': name, 'axis': False, 'bindings': [{'control': control, 'scale': 1., 'deadzone': 0.}]})
write('physics_stress_lab.judasproj', project)
print('Authored ordinary physics lab:', OUT)

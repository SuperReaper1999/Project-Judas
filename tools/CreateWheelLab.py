#!/usr/bin/env python3
"""Build reproducible ordinary M81 wheel content; no native demo switches.

All vehicle layouts, controls, camera policy and autopilot live in project JS.
The physical wheel components reference ordinary dynamic chassis bodies.
"""
from pathlib import Path
import json
import math
import shutil

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'projects/wheel_lab'
for directory in ('Assets/models', 'Assets/scripts', 'Assets/ui', 'Assets/fonts',
                  'Assets/prefabs', 'Assets/worlds', 'Scenes'):
    (OUT / directory).mkdir(parents=True, exist_ok=True)
IDS = {name: '81' * 12 + f'{n:08x}' for n, name in enumerate(
    ('wheel', 'controller', 'math', 'ui', 'font', 'buggy', 'rover', 'world', 'payload', 'contracts'), 1)}


def vec(value):
    return ' '.join(f'{n:.9g}' for n in value)


def metadata(path, name, kind):
    (OUT / (path + '.judasmeta')).write_text(
        f'JudasAssetMeta 1\nid "{IDS[name]}"\ntype {kind}\nsource ""\n')


def rotation(axis, angle):
    return (math.cos(angle / 2), *(n * math.sin(angle / 2) for n in axis))


def multiply(a, b):
    w, x, y, z = a
    v, i, j, k = b
    return (w*v-x*i-y*j-z*k, w*i+x*v+y*k-z*j,
            w*j-x*k+y*v+z*i, w*k+x*j-y*i+z*v)


def turn(q, value):
    result = multiply(multiply(q, (0, *value)), (q[0], -q[1], -q[2], -q[3]))
    return result[1:]


def plus(a, b):
    return tuple(x + y for x, y in zip(a, b))


def surface(x, z, mode):
    if mode == 'radial':
        radius = 80
        y = math.sqrt(radius*radius-x*x-z*z) - radius
        up = (x/radius, (y+radius)/radius, z/radius)
        # Shortest rotation from local +Y to the actual planetary normal.
        half = math.sqrt((1 + up[1]) / 2)
        q = (half, up[2]/(2*half), 0, -up[0]/(2*half))
        return (x, y, z), q
    q = rotation((0, 0, 1), .65) if mode == 'oblique' else (1, 0, 0, 0)
    return turn(q, (x, 0, z)), q


def render(half, color):
    return (f'  render box\n  render.half-extents {vec(half)}\n'
            f'  render.color {vec(color)}\n')


def body(half, mass=0, friction=.75):
    return (f'  body {"dynamic" if mass else "static"} box\n'
            f'  body.half-extents {vec(half)}\n  body.mass {mass or 1}\n'
            f'  body.friction {friction}\n  body.restitution 0\n')


def object_(identity, name, position, fields, q=(1, 0, 0, 0), parent=0):
    defaults = {}
    if '  render ' in fields:
        defaults.update({'render.half-extents': '.5 .5 .5', 'render.radius': '.5',
                         'render.color': '.8 .8 .8', 'render.alpha': '1',
                         'render.secondary-color': '.8 .8 .8', 'render.secondary-alpha': '1',
                         'render.mesh-asset': '""', 'render.texture-asset': '""'})
    if '  body ' in fields:
        defaults.update({'body.half-extents': '.5 .5 .5', 'body.radius': '.5',
                         'body.terrain': '""', 'body.mass': '1', 'body.friction': '.75',
                         'body.restitution': '0', 'body.initial-velocity': '0 0 0',
                         'body.pickable': 'false', 'body.managed': 'false',
                         'body.compound-count': '0'})
    for key, value in defaults.items():
        if f'  {key} ' not in fields:
            fields += f'  {key} {value}\n'
    parent_line = f'  parent {parent}\n' if parent else ''
    return (f'object {identity} {json.dumps(name)}\n{parent_line}'
            f'  position {vec(position)}\n  rotation {vec(q)}\n  scale 1 1 1\n'
            + fields + 'end\n')


def settings(name, next_id=3000):
    return (f'JudasScene 3\nsettings\n  name {json.dumps(name)}\n'
            '  world-origin 0 0 0\n  sun-color 1 .97 .91\n'
            '  sun-direction -.3 .8 .5\n  ambient .3 .34 .38\n'
            f'  fluid-scale 1\n  fidelity-policy none\n  next-id {next_id}\nend\n')


def wheel(chassis, mount, stiffness):
    values = {'enabled': 'true', 'chassis': str(chassis), 'mount': vec(mount),
              'orientation': '1 0 0 0', 'radius': '.38', 'minLength': '.1',
              'restLength': '.4', 'maxLength': '.65', 'springStiffness': str(stiffness),
              'damping': '1600', 'spinInertia': '1.2', 'friction': '1',
              'rollingResistance': '.012', 'collisionLayer': '0',
              'collisionMask': str(2**64-1), 'requiredTags': '0', 'excludedTags': '0'}
    return ''.join(f'  wheel.{key} {value}\n' for key, value in values.items())


def assembly(root_id, position, q=(1, 0, 0, 0), rover=False):
    mass, color = (950, (.93, .50, .14)) if rover else (650, (.10, .67, .71))
    half = (.85, .24, 1.65 if rover else 1.2)
    rows = [object_(root_id, 'Six-wheel exploration rover' if rover else 'Four-wheel buggy',
                    position, render(half, color) + body(half, mass), q)]
    axles = (-1.35, 0, 1.35) if rover else (-.95, .95)
    index = 1
    for z in axles:
        for side in (-1, 1):
            mount = (side * 1.02, -.15, z)
            rows.append(object_(root_id + index, 'Independent suspension and rolling contact',
                                plus(mount, (0, -.4, 0)),
                                f'  render mesh\n  render.mesh-asset "{IDS["wheel"]}"\n'
                                '  render.color .055 .075 .095\n' + wheel(root_id, mount, 16000 if rover else 14000),
                                parent=root_id))
            rows.append(object_(root_id + 30 + index, 'Visible rolling marker / no collider',
                                (side*.155, 0, 0), render((.018, .035, .28), (.72, .78, .81)),
                                parent=root_id + index))
            index += 1
    # Ordinary visual children; their geometry does not supply collision.
    rows.append(object_(root_id+20, 'Cabin visual / chassis collision stays separate',
                        (0, .5, .20), render((.58, .30, .55), (.25, .36, .45)), parent=root_id))
    rows.append(object_(root_id+21, 'Forward marker', (0, .31, -half[2]+.06),
                        render((.60, .065, .035), (.96, .89, .42)), parent=root_id))
    rows.append(object_(root_id+22, 'Rear marker', (0, .31, half[2]-.06),
                        render((.60, .065, .035), (.80, .19, .16)), parent=root_id))
    return ''.join(rows)


# Original, flat-shaded 20-sided wheel. Local axle is +X; radius matches .38 m.
# Mesh width (.26 m) is presentation only: the query contact is an isotropic sphere.
vertices, faces = [], []
for x, radius in ((-.13, .32), (-.10, .38), (.10, .38), (.13, .32)):
    for i in range(20):
        angle = i * 2*math.pi/20
        vertices.append((x, radius*math.cos(angle), radius*math.sin(angle)))
for ring in range(3):
    for i in range(20):
        j = (i+1) % 20
        faces.append((ring*20+i+1, ring*20+j+1, (ring+1)*20+j+1, (ring+1)*20+i+1))
faces.extend((tuple(range(20, 0, -1)), tuple(range(61, 81))))
(OUT/'Assets/models/wheel.obj').write_text('# Original Judas M81 wheel, CC0\ns off\n'
    + ''.join('v ' + vec(v) + '\n' for v in vertices)
    + ''.join('f ' + ' '.join(map(str, f)) + '\n' for f in faces))
(OUT/'Assets/models/LICENSE.txt').write_text('Original M81 wheel mesh by Project Judas. Dedicated to the public domain under CC0 1.0.\n')
metadata('Assets/models/wheel.obj', 'wheel', 'mesh')
shutil.copyfile(ROOT/'projects/bending_lab/Assets/scripts/math.js', OUT/'Assets/scripts/math.js')
metadata('Assets/scripts/math.js', 'math', 'script')
metadata('Assets/scripts/controller.js', 'controller', 'script')
metadata('Assets/scripts/contracts.js', 'contracts', 'script')
for name in ('DejaVuSans.ttf', 'LICENSE.txt'):
    shutil.copyfile(ROOT/'projects/bending_lab/Assets/fonts'/name, OUT/'Assets/fonts'/name)
metadata('Assets/fonts/DejaVuSans.ttf', 'font', 'font')
metadata('Assets/ui/lab.judasui', 'ui', 'ui')
metadata('Assets/prefabs/buggy.judasprefab', 'buggy', 'prefab')
metadata('Assets/prefabs/rover.judasprefab', 'rover', 'prefab')
metadata('Assets/prefabs/payload.judasprefab', 'payload', 'prefab')
metadata('Assets/worlds/lab.judasworld', 'world', 'world')

ui = (ROOT/'projects/bending_lab/Assets/ui/lab.judasui').read_text()
ui = ui.replace('8080808080808080808080800000000d', IDS['font'])
ui = ui.replace('BENDING LAB PAUSED', 'WHEEL LAB PAUSED').replace('bending-current', 'wheel-current')
for old, new in (
    ('JUDAS / ELASTICITY, PLASTICITY & FAILURE', 'JUDAS / WHEELS, SUSPENSION & TRACTION'),
    ('A elastic | B plastic | C flexible connections | D physical dent', 'Four independent wheels / six independent wheels / three gravity scenes'),
    ('B: beam load/unload | H: increasing C load | G/T: carry/throw press', 'W/S torque | A/D steer | Space brake | Tab vehicle | V autopilot'),
    ('Load the beams, unload them and compare recovery. Orange press is a real rigid body.', 'Suspension loads and tyre impulses drive the actual chassis. No scripted motion.'),
    ('WASD/mouse | B beam load | H failure load | Click impact | G/T carry/throw | P prefab | F3 annex | 1/2 scenes | F5/F8 save/load | F9 reset | Esc',
     'WASD/Space drive | Tab vehicle | V watch | B load | H grip | P prefab | F3 garage | 1/2/3 gravity | F5/F8 save/load | R reset | Esc'),
):
    ui = ui.replace(old, new)
ui = ui.replace('"cross_h" "canvas" 1 0 1', '"cross_h" "canvas" 1 0 0')
ui = ui.replace('"cross_v" "canvas" 1 0 1', '"cross_v" "canvas" 1 0 0')
(OUT/'Assets/ui/lab.judasui').write_text(ui)

actions = {'brake': 'key:Space', 'select': 'key:Tab', 'autopilot': 'key:V',
           'payload': 'key:B', 'grip': 'key:H', 'spawn': 'key:P', 'region': 'key:F3',
           'flat': 'key:1', 'oblique': 'key:2', 'radial': 'key:3',
           'save_game': 'key:F5', 'load_game': 'key:F8', 'pause': 'key:Escape',
           'restart': 'key:R', 'view': 'key:C', 'ui_click': 'mouse:Left',
           'ui_activate': 'key:Return', 'ui_up': 'key:Up', 'ui_down': 'key:Down'}
axes = {'throttle': [('key:S', -1), ('key:W', 1)],
        'steer': [('key:D', -1), ('key:A', 1)],
        'look_x': [('mouse:dx', 1)], 'look_y': [('mouse:dy', 1)]}
mapping = f'1 {len(actions)+len(axes)} ' + ''.join(
    f'"{name}" 0 1 "{binding}" 1 0 ' for name, binding in actions.items()) + ''.join(
    f'"{name}" 1 {len(bindings)} ' + ''.join(f'"{binding}" {scale} 0 ' for binding, scale in bindings)
    for name, bindings in axes.items())
(OUT/'wheel_lab.judasproj').write_text(
    'JudasProject 1\nname "Wheel Lab"\nstartup-scene "Scenes/flat.judas"\n'
    'assets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\nlegacy-gameplay "false"\n'
    'classification ' + json.dumps('JudasClassification 1 1 1 0 "payload" 1 1 0 "Default" 1 1 0 "Default"')
    + '\ninput-map ' + json.dumps(mapping) + '\nworld-manifest "' + IDS['world'] + '"\n')

for mode in ('flat', 'oblique', 'radial'):
    base, q = surface(0, 0, mode)
    field = ('  gravity radial 9.81\n  gravity.region sphere 200\n' if mode == 'radial'
             else '  gravity uniform 9.81\n  gravity.region sphere 250\n')
    rows = [object_(1, 'Ordinary ' + ('RadicalGravity' if mode == 'radial' else 'uniform gravity'),
                    (0, -80, 0) if mode == 'radial' else base, field, q),
            object_(10, 'Project-owned driver, camera and lab UI', (0, 0, 0),
                    '  scripts 1\n  script.0.id 1\n'
                    f'  script.0.asset "{IDS["controller"]}"\n  script.0.enabled true\n'
                    '  script.0.properties "{}"\n'
                    f'  ui.asset "{IDS["ui"]}"\n  ui.name "lab"\n  ui.enabled true\n')]
    if mode == 'radial':
        rows.append(object_(3, 'Real spherical collision surface, radius 80 m', (0, -80, 0),
                            '  render sphere\n  render.radius 80\n  render.color .13 .22 .24\n'
                            '  body static sphere\n  body.radius 80\n'))
    else:
        rows.append(object_(3, 'Ordinary collision deck', turn(q, (0, -.35, -5)),
                            render((24, .35, 40), (.13, .20, .24)) + body((24, .35, 40)), q))
    for identity, x, rover in ((100, -6, False), (200, 6, True)):
        position, attitude = surface(x, 16, mode)
        rows.append(assembly(identity, plus(position, turn(attitude, (0, .94, 0))), attitude, rover))
    for index, (x, z, height, pitch) in enumerate((
        (-6, 10, .12, 0), (-5.2, 5, .17, 0), (-6.7, 0, .15, 0),
        (6, 10, .13, 0), (5.3, 5, .19, 0), (6.7, 0, .18, 0),
        (-6, -7, .35, .12), (6, -7, .35, .12),
    )):
        position, attitude = surface(x, z, mode)
        half = (1.4, height, 2.5 if pitch else .18)
        rows.append(object_(400+index, 'Uneven load-bearing track obstacle',
                            plus(position, turn(attitude, (0, height, 0))),
                            render(half, (.43, .51, .53)) + body(half),
                            multiply(attitude, rotation((1, 0, 0), pitch))))
    for index, z in enumerate(range(-22, 24, 4)):
        position, attitude = surface(0, z, mode)
        rows.append(object_(500+index, 'Track center marking', plus(position, turn(attitude, (0, .02, 0))),
                            render((.08, .015, 1.2), (.85, .77, .34)), attitude))
    for index, x in enumerate((-12, 12)):
        position, attitude = surface(x, -8, mode)
        rows.append(object_(600+index, 'Freestanding track guide',
                            plus(position, turn(attitude, (0, 1.8, 0))),
                            render((.15, 1.8, .15), (.26, .63, .71)) + body((.15, 1.8, .15)), attitude))
    seed, attitude = surface(-6, 24, mode)
    rows.append(object_(12, 'Compatibility camera seed only', plus(seed, turn(attitude, (0, 4, 0))),
                        '  player-start first-person\n  player-start.yaw 0\n', attitude))
    (OUT/'Scenes'/f'{mode}.judas').write_text(settings('Wheel lab / ' + mode) + '\n'.join(rows))

(OUT/'Scenes/contracts.judas').write_text((OUT/'Scenes/flat.judas').read_text()
    + object_(11, 'Public API positive and negative fixture', (0, 0, 0),
              '  scripts 1\n  script.0.id 1\n'
              f'  script.0.asset "{IDS["contracts"]}"\n  script.0.enabled true\n  script.0.properties "{{}}"\n'))

for rover in (False, True):
    name = 'rover' if rover else 'buggy'
    (OUT/f'Assets/prefabs/{name}.judasprefab').write_text(
        settings('Reusable ' + name + ' assembly', 40) + assembly(1, (0, 0, 0), rover=rover))
(OUT/'Assets/prefabs/payload.judasprefab').write_text(settings('Ordinary 225 kg test payload', 2)
    + object_(1, 'Finite-mass test payload', (0, 0, 0),
              render((.42, .20, .48), (.69, .29, .62)) + body((.42, .20, .48), 225)))
# A real self-contained streamed assembly, not a second native test mode.
(OUT/'Scenes/garage.judas').write_text(settings('Streamed independent wheel assembly', 50)
    + assembly(1, (35, .94, 4))
    + object_(40, 'Garage deck', (35, -.35, 4), render((6, .35, 6), (.24, .29, .32)) + body((6, .35, 6))))
(OUT/'Assets/worlds/lab.judasworld').write_text(
    'JudasWorld 1\nbudget 2 8 2 8388608 8388608 8388608 2097152\n'
    'region "garage" "Scenes/garage.judas" 0 0 0 0 0 0 1 35 3 4 1 "snapshot" 262144 ""\n')
print(OUT)

#!/usr/bin/env python3
"""Reproducible ordinary M82 content. Driver policy belongs to project JS.

The graph contains finite shafts, torque sources and mechanical connections;
the engine never knows that this particular collection is a buggy or a lathe.
"""
from pathlib import Path
import json
import math
import shutil

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'projects/powertrain_lab'
for directory in ('Assets/models', 'Assets/scripts', 'Assets/ui', 'Assets/fonts',
                  'Assets/prefabs', 'Assets/worlds', 'Scenes'):
    (OUT / directory).mkdir(parents=True, exist_ok=True)
IDS = {name: '82' * 12 + f'{n:08x}' for n, name in enumerate(
    ('wheel', 'controller', 'math', 'ui', 'font', 'buggy', 'rover', 'world', 'payload', 'contracts'), 1)}


def vec(values):
    return ' '.join(f'{value:.9g}' for value in values)


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
    return multiply(multiply(q, (0, *value)), (q[0], -q[1], -q[2], -q[3]))[1:]


def plus(a, b):
    return tuple(x + y for x, y in zip(a, b))


def surface(x, z, mode):
    if mode == 'radial':
        radius = 80
        y = math.sqrt(radius*radius-x*x-z*z) - radius
        up = (x/radius, (y+radius)/radius, z/radius)
        half = math.sqrt((1 + up[1]) / 2)
        return (x, y, z), (half, up[2]/(2*half), 0, -up[0]/(2*half))
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
    return (f'object {identity} {json.dumps(name)}\n'
            + (f'  parent {parent}\n' if parent else '')
            + f'  position {vec(position)}\n  rotation {vec(q)}\n  scale 1 1 1\n'
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


def graph_line(graph):
    return '  powertrain.graph ' + json.dumps(json.dumps(graph, separators=(',', ':'))) + '\n'


def shaft(name, kind='internal', inertia=.2, speed=0, drag=0, target=0, bearing=0):
    return {'name': name, 'kind': kind, 'inertia': inertia,
            'initialAngularVelocity': speed, 'viscousDrag': drag,
            'target': str(target), 'bearing': str(bearing), 'axis': [1, 0, 0]}


def source(name, shaft_index, kind, torque=200, power=100000, starter=0, curve=()):
    return {'name': name, 'shaft': shaft_index, 'kind': kind,
            'torqueCurve': list(curve), 'maxTorque': torque, 'maxPower': power,
            'starterTorque': starter}


def link(name, kind, input_, output, ratio=1, torque=500, output_b=0):
    return {'name': name, 'kind': kind, 'input': input_, 'output': output,
            'outputB': output_b, 'ratio': ratio, 'maxTorque': torque}


def brake(name, shaft_index, torque=750):
    return {'name': name, 'shaft': shaft_index, 'maxTorque': torque}


def combustion_graph(root):
    return {'version': 1, 'enabled': True, 'shafts': [
        shaft('crank', inertia=.28, speed=100, drag=.018, bearing=root),
        shaft('gearbox', inertia=.07, bearing=root),
        shaft('carrier', inertia=.1, bearing=root),
        shaft('left', 'wheel', target=root+3), shaft('right', 'wheel', target=root+4)],
        'sources': [source('engine', 0, 'combustion', starter=42, curve=(
            [0, 0], [600, 0], [900, 95], [1800, 145], [3200, 160],
            [4800, 125], [6200, 40], [6800, 0]))],
        'links': [link('gearbox', 'gear', 0, 1, 1/4.5),
                  link('clutch', 'clutch', 1, 2, torque=1000),
                  link('differential', 'differential', 2, 3, 1/3.5, output_b=4)],
        'brakes': [brake('left', 3), brake('right', 4)]}


def electric_graph(root):
    return {'version': 1, 'enabled': True,
            'shafts': [shaft(f'wheel{i+1}', 'wheel', target=root+i+1) for i in range(6)],
            'sources': [source(f'motor{i+1}', i, 'electric', torque=180, power=12000) for i in range(6)],
            'links': [], 'brakes': [brake(f'wheel{i+1}', i) for i in range(6)]}


def machine_graph(root):
    return {'version': 1, 'enabled': True,
            'shafts': [shaft('motor', inertia=.15, drag=.05, bearing=root),
                       shaft('flywheel', 'body', target=root+1, bearing=root)],
            'sources': [source('motor', 0, 'electric', torque=18, power=1000)],
            'links': [link('reduction', 'gear', 0, 1, .2)],
            'brakes': [brake('load', 1, 180)]}


def assembly(root, position, q=(1, 0, 0, 0), rover=False):
    mass, color = (950, (.10, .68, .73)) if rover else (650, (.93, .47, .16))
    half = (.85, .24, 1.65 if rover else 1.2)
    graph = electric_graph(root) if rover else combustion_graph(root)
    rows = [object_(root, 'Independent electric rover' if rover else 'Combustion / gearbox / differential buggy',
                    position, render(half, color) + body(half, mass) + graph_line(graph), q)]
    axles = (-1.35, 0, 1.35) if rover else (-.95, .95)
    index = 1
    for z in axles:
        for side in (-1, 1):
            mount = (side * 1.02, -.15, z)
            rows.append(object_(root + index, 'Ordinary M81 suspension and tyre terminal',
                                plus(mount, (0, -.4, 0)),
                                f'  render mesh\n  render.mesh-asset "{IDS["wheel"]}"\n'
                                '  render.color .055 .075 .095\n' + wheel(root, mount, 16000 if rover else 14000),
                                parent=root))
            rows.append(object_(root + 30 + index, 'Rolling marker / no duplicate collider',
                                (side*.155, 0, 0), render((.018, .035, .28), (.72, .78, .81)),
                                parent=root + index))
            index += 1
    rows.append(object_(root+20, 'Cabin visual / physics belongs to chassis',
                        (0, .5, .20), render((.58, .30, .55), (.25, .36, .45)), parent=root))
    rows.append(object_(root+21, 'Forward marker', (0, .31, -half[2]+.06),
                        render((.60, .065, .035), (.96, .89, .42)), parent=root))
    rows.append(object_(root+22, 'Rear marker', (0, .31, half[2]-.06),
                        render((.60, .065, .035), (.80, .19, .16)), parent=root))
    return ''.join(rows)


def machine(root, position, q):
    rows = [object_(root, 'Electric shaft machine / fixed bearing', position,
                    render((1.05, .15, 1.05), (.29, .30, .37)) + body((1.05, .15, 1.05))
                    + graph_line(machine_graph(root)), q),
            object_(root+1, 'Physical hinged flywheel', (0, 1.45, 0),
                    render((.10, .83, .83), (.68, .38, .61)) + body((.10, .83, .83), 18), parent=root),
            object_(root+2, 'Flywheel hinge / axis local X', (0, 0, 0),
                    f'  joint 1\n  joint.body-a {root+1}\n  joint.body-b {root}\n'
                    '  joint.anchor-a 0 0 0\n  joint.anchor-b 0 1.45 0\n'
                    '  joint.frame-a 1 0 0 0\n  joint.frame-b 1 0 0 0\n'
                    '  joint.enabled true\n  joint.limits false\n  joint.motor false\n  joint.spring false\n'
                    '  joint.lower -1\n  joint.upper 1\n  joint.speed 0\n  joint.max-force 10\n'
                    '  joint.rest 0\n  joint.stiffness 10\n  joint.damping 1\n', parent=root),
            object_(root+3, 'Physical flywheel rotation marker', (.115, .54, 0),
                    render((.03, .18, .05), (.98, .83, .33)), parent=root+1),
            object_(root+4, 'Bearing stand', (0, .78, .97), render((.14, .65, .14), (.44, .49, .54)), parent=root)]
    return ''.join(rows)


def create():
    shutil.copyfile(ROOT/'projects/wheel_lab/Assets/models/wheel.obj', OUT/'Assets/models/wheel.obj')
    shutil.copyfile(ROOT/'projects/wheel_lab/Assets/models/LICENSE.txt', OUT/'Assets/models/LICENSE.txt')
    shutil.copyfile(ROOT/'projects/wheel_lab/Assets/scripts/math.js', OUT/'Assets/scripts/math.js')
    for name in ('DejaVuSans.ttf', 'LICENSE.txt'):
        shutil.copyfile(ROOT/'projects/wheel_lab/Assets/fonts'/name, OUT/'Assets/fonts'/name)
    for path, name, kind in (
        ('Assets/models/wheel.obj', 'wheel', 'mesh'), ('Assets/scripts/math.js', 'math', 'script'),
        ('Assets/scripts/controller.js', 'controller', 'script'), ('Assets/scripts/contracts.js', 'contracts', 'script'),
        ('Assets/fonts/DejaVuSans.ttf', 'font', 'font'), ('Assets/ui/lab.judasui', 'ui', 'ui'),
        ('Assets/prefabs/buggy.judasprefab', 'buggy', 'prefab'), ('Assets/prefabs/rover.judasprefab', 'rover', 'prefab'),
        ('Assets/prefabs/payload.judasprefab', 'payload', 'prefab'), ('Assets/worlds/lab.judasworld', 'world', 'world')):
        metadata(path, name, kind)
    ui = (ROOT/'projects/wheel_lab/Assets/ui/lab.judasui').read_text().replace('81818181818181818181818100000005', IDS['font'])
    ui = ui.replace('JUDAS / WHEELS, SUSPENSION & TRACTION', 'JUDAS / TORQUE, GEARS & MECHANICAL LOADS')
    ui = ui.replace('Four independent wheels / six independent wheels / three gravity scenes', 'Combustion transmission / independent electric wheels / physical flywheel')
    ui = ui.replace('W/S torque | A/D steer | Space brake | Tab vehicle | V autopilot', 'W/S throttle | Q/E gear | F clutch | A/D steer | Space brake | Tab machine')
    ui = ui.replace('Suspension loads and tyre impulses drive the actual chassis. No scripted motion.', 'Finite inertia and mechanical constraints transfer real torque. RPM is measured.')
    ui = ui.replace('WASD/Space drive | Tab vehicle | V watch | B load | H grip | P prefab | F3 garage | 1/2/3 gravity | F5/F8 save/load | R reset | Esc',
                    'WASD/Space | Q/E gear | F clutch | Tab select | V watch | B load | H grip | P prefab | F3 garage | 1/2/3 | F5/F8 | R | Esc')
    ui = ui.replace('WHEEL LAB PAUSED', 'POWERTRAIN LAB PAUSED').replace('wheel-current', 'powertrain-current')
    (OUT/'Assets/ui/lab.judasui').write_text(ui)
    actions = {'brake': 'key:Space', 'select': 'key:Tab', 'autopilot': 'key:V',
               'payload': 'key:B', 'grip': 'key:H', 'spawn': 'key:P', 'region': 'key:F3',
               'gear_down': 'key:Q', 'gear_up': 'key:E', 'clutch': 'key:F',
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
    (OUT/'powertrain_lab.judasproj').write_text(
        'JudasProject 1\nname "Powertrain Lab"\nstartup-scene "Scenes/flat.judas"\n'
        'assets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\nlegacy-gameplay "false"\n'
        'classification ' + json.dumps('JudasClassification 1 1 1 0 "payload" 1 1 0 "Default" 1 1 0 "Default"')
        + '\ninput-map ' + json.dumps(mapping) + '\nworld-manifest "' + IDS['world'] + '"\n')
    for mode in ('flat', 'oblique', 'radial'):
        base, q = surface(0, 0, mode)
        field = ('  gravity radial 9.81\n  gravity.region sphere 200\n' if mode == 'radial'
                 else '  gravity uniform 9.81\n  gravity.region sphere 250\n')
        rows = [object_(1, 'Ordinary ' + ('RadicalGravity' if mode == 'radial' else 'uniform gravity'),
                        (0, -80, 0) if mode == 'radial' else base, field, q),
                object_(10, 'Project-owned power controls, camera and lab UI', (0, 0, 0),
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
        machine_base, machine_q = surface(0, 10, mode)
        rows.append(machine(300, plus(machine_base, turn(machine_q, (0, .15, 0))), machine_q))
        for index, (x, z, height, pitch) in enumerate((
            (-6, 10, .12, 0), (-5.2, 5, .17, 0), (-6.7, 0, .15, 0),
            (6, 10, .13, 0), (5.3, 5, .19, 0), (6.7, 0, .18, 0),
            (-6, -7, .35, .12), (6, -7, .35, .12))):
            position, attitude = surface(x, z, mode)
            half = (1.4, height, 2.5 if pitch else .18)
            rows.append(object_(400+index, 'Uneven load-bearing track obstacle',
                                plus(position, turn(attitude, (0, height, 0))),
                                render(half, (.43, .51, .53)) + body(half),
                                multiply(attitude, rotation((1, 0, 0), pitch))))
        for index, z in enumerate(range(-22, 24, 4)):
            position, attitude = surface(0, z, mode)
            rows.append(object_(500+index, 'Track centre marking', plus(position, turn(attitude, (0, .02, 0))),
                                render((.08, .015, 1.2), (.85, .77, .34)), attitude))
        seed, attitude = surface(-6, 24, mode)
        rows.append(object_(12, 'Compatibility camera seed only', plus(seed, turn(attitude, (0, 4, 0))),
                            '  player-start first-person\n  player-start.yaw 0\n', attitude))
        (OUT/'Scenes'/f'{mode}.judas').write_text(settings('Powertrain lab / ' + mode) + '\n'.join(rows))
    (OUT/'Scenes/contracts.judas').write_text((OUT/'Scenes/flat.judas').read_text()
        + object_(11, 'Public graph API positive and negative fixture', (0, 0, 0),
                  '  scripts 1\n  script.0.id 1\n'
                  f'  script.0.asset "{IDS["contracts"]}"\n  script.0.enabled true\n  script.0.properties "{{}}"\n'))
    for rover in (False, True):
        name = 'rover' if rover else 'buggy'
        (OUT/f'Assets/prefabs/{name}.judasprefab').write_text(settings('Reusable powertrain ' + name, 40)
            + assembly(1, (0, 0, 0), rover=rover))
    (OUT/'Assets/prefabs/payload.judasprefab').write_text(settings('Ordinary 225 kg test payload', 2)
        + object_(1, 'Finite-mass test payload', (0, 0, 0),
                  render((.42, .20, .48), (.69, .29, .62)) + body((.42, .20, .48), 225)))
    (OUT/'Scenes/garage.judas').write_text(settings('Streamed complete mechanical assembly', 50)
        + assembly(1, (35, .94, 4))
        + object_(40, 'Garage deck', (35, -.35, 4), render((6, .35, 6), (.24, .29, .32)) + body((6, .35, 6))))
    (OUT/'Assets/worlds/lab.judasworld').write_text(
        'JudasWorld 1\nbudget 2 8 2 8388608 8388608 8388608 2097152\n'
        'region "garage" "Scenes/garage.judas" 0 0 0 0 0 0 1 35 3 4 1 "snapshot" 262144 ""\n')
    print(OUT)


if __name__ == '__main__':
    create()

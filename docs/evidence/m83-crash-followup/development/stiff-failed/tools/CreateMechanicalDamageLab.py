#!/usr/bin/env python3
"""Ordinary M83 content: real material, finite carriers and mechanical links.

The controls apply physical forces/impulses. They never deform a mesh, move a
wheel mount, mark a connection broken or synthesize a loss of drive.
"""
from pathlib import Path
import ast
import json
import math
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'projects/damage_lab'
arguments = [arg for arg in sys.argv[1:] if arg != '--crash-only']
BAKER = Path(arguments[0] if arguments else ROOT / 'build/judas_deformable_bake')
IDS = {name: '83' * 12 + f'{n:08x}' for n, name in enumerate(
    ('wheel', 'controller', 'math', 'ui', 'font', 'arm', 'section', 'outer',
     'inner', 'buggy', 'bench', 'world', 'ram', 'crash_car'), 1)}

# Reuse the current ordinary scene-format helpers, without executing another
# project's generator or modifying any accepted lab.
tree = ast.parse((ROOT / 'tools/CreatePowertrainLab.py').read_text())
names = {'vec', 'metadata', 'rotation', 'multiply', 'turn', 'plus', 'render',
         'body', 'object_', 'settings', 'wheel', 'shaft', 'source', 'link',
         'brake', 'graph_line'}
exec(compile(ast.Module([node for node in tree.body
                        if isinstance(node, ast.FunctionDef) and node.name in names],
                       type_ignores=[]), '<ordinary scene helpers>', 'exec'), globals())


def script(properties=None):
    return ('  scripts 1\n  script.0.id 1\n'
            f'  script.0.asset "{IDS["controller"]}"\n  script.0.enabled true\n'
            '  script.0.properties ' + json.dumps(json.dumps(properties or {})) + '\n')


def material(asset, density, compliance, attachments, plastic=False, limit=0):
    values = {'asset': IDS[asset], 'enabled': 1, 'selfContact': 0, 'substeps': 4,
              'iterations': 4, 'collisionLayer': 1, 'collisionMask': 1,
              'material.density': density, 'material.stretchCompliance': 1e-6,
              'material.shearCompliance': compliance, 'material.bendCompliance': .0004,
              'material.volumeCompliance': 1e-7, 'material.damping': 2,
              'material.thickness': .015, 'material.friction': .4,
              'material.airDrag': 0, 'material.yieldStrain': .04 if plastic else 0,
              'material.plasticRate': 1.5 if plastic else 0,
              'material.maximumPlasticStrain': .45, 'airVelocity': '0 0 0'}
    text = ''.join(f'  deformable.{key} {json.dumps(str(value))}\n'
                   for key, value in values.items())
    rows = [str(len(attachments))]
    for group, target, offset in attachments:
        rows += [json.dumps(group), '1' if target else '0', str(target), '""', vec(offset), '1']
    text += '  deformable.attachments ' + json.dumps(' '.join(rows)) + '\n'
    if limit:
        text += '  deformable.attachmentLimits ' + json.dumps(f'1 "right" {limit} 0') + '\n'
    return text + '  render.materials ' + json.dumps(
        f'2 "{IDS["outer"]}" 0 "{IDS["inner"]}" 0') + '\n'


def connection(material_owner, attachment='right', root_attachment='left'):
    return {'owner': str(material_owner), 'attachment': attachment,
            'rootAttachment': root_attachment}


def assembly_body(half, mass=0):
    # Attached material cannot collide with its own finite support at the same
    # pin. Assembly filtering is explicit content, not an object-name rule.
    return body(half, mass) + '  body.collision-layer 1\n  body.collision-mask 1\n'


def buggy_graph(root):
    shafts = [shaft('motor', inertia=.3, drag=.01, bearing=root)]
    links = []
    for index in range(4):
        shafts.append(shaft(f'wheel{index+1}', 'wheel', target=root+1+index))
        row = link(f'drive{index+1}', 'clutch', 0, index+1, .2, 80)
        row['connection'] = connection(root+20+index, 'right' if index % 2 else 'left',
                                       'left' if index % 2 else 'right')
        links.append(row)
    return {'version': 1, 'enabled': True, 'shafts': shafts,
            'sources': [source('motor', 0, 'electric', 30, 1800)],
            'links': links, 'brakes': [brake(f'wheel{i+1}', i+1, 60) for i in range(4)]}


def buggy(root, position, q=(1, 0, 0, 0), strong=False):
    color = (.16, .70, .56) if strong else (.94, .49, .16)
    rows = [object_(root, 'High-strength control buggy' if strong else 'Yielding suspension buggy',
                    position, render((.62, .18, 1.05), color) + assembly_body((.62, .18, 1.05), 100)
                    + graph_line(buggy_graph(root)), q)]
    for index, (z, side) in enumerate(((-.8, -1), (-.8, 1), (.8, -1), (.8, 1))):
        carrier, arm = root+10+index, root+20+index
        center, mount = (side*.78, -.05, z), (side*1.38, -.05, z)
        tip, root_group = ('right', 'left') if side > 0 else ('left', 'right')
        rows.append(object_(carrier, 'Finite suspension carrier / wheel bearing', mount,
                            render((.12, .12, .20), (.38, .45, .51)) + assembly_body((.12, .12, .20), 8),
                            parent=root))
        attachment = [(root_group, root, center), (tip, carrier, (-side*.6, 0, 0))]
        fields = material('arm', 80, .00001 if strong else .00005, attachment,
                          plastic=not strong)
        fields += '  deformable.attachmentLimits ' + json.dumps(
            f'1 "{tip}" {3000 if strong else 350} 0') + '\n'
        rows.append(object_(arm, 'Strong arm' if strong else 'Plastic arm / limited carrier mount',
                            center, render((.6, .13, .18), color) + fields, parent=root))
        fields = wheel(carrier, (0, 0, 0), 1800)
        fields = fields.replace('wheel.collisionLayer 0', 'wheel.collisionLayer 1').replace(
            f'wheel.collisionMask {2**64-1}', 'wheel.collisionMask 1')
        for a, b in (('wheel.radius .38', 'wheel.radius .32'),
                     ('wheel.damping 1600', 'wheel.damping 120'),
                     ('wheel.spinInertia 1.2', 'wheel.spinInertia .2')):
            fields = fields.replace(a, b)
        rows.append(object_(root+1+index, 'Wheel / bearing follows actual carrier body',
                            (0, -.4, 0), f'  render mesh\n  render.mesh-asset "{IDS["wheel"]}"\n'
                            '  render.color .06 .075 .09\n' + fields, parent=carrier))
        rows.append(object_(root+40+index, 'Wheel rotation stripe', (side*.155, 0, 0),
                            render((.02, .04, .24), (.90, .84, .59)), parent=root+1+index))
    rows.append(object_(root+30, 'Cabin visual', (0, .42, .15),
                        render((.40, .23, .48), (.24, .34, .42)), parent=root))
    rows.append(object_(root+31, 'Forward marker', (0, .21, -.99),
                        render((.5, .04, .035), (.98, .90, .52)), parent=root))
    return ''.join(rows)


def bench(root, position, q=(1, 0, 0, 0), strong=False, fractured=False):
    color = (.15, .69, .60) if strong else ((.65, .34, .81) if fractured else (.96, .52, .14))
    carrier, arm, wheel_id = root+1, root+2, root+3
    row = link('drive', 'clutch', 0, 1, .25, 40)
    row['connection'] = connection(arm)
    graph = {'version': 1, 'enabled': True,
             'shafts': [shaft('motor', inertia=.25, drag=.01, bearing=root),
                        shaft('wheel', 'wheel', target=wheel_id)],
             'sources': [source('motor', 0, 'electric', 8, 250)],
             'links': [row], 'brakes': [brake('wheel', 1, 30)]}
    rows = [object_(root, 'Cohesive load path bench' if fractured else
                    ('Strong material comparison bench' if strong else 'Plastic suspension bench'),
                    position, render((.65, 1.1, .45), (.30, .37, .43))
                    + assembly_body((.65, 1.1, .45)) + graph_line(graph), q)]
    tip = (1.80, -.35, 0)
    rows.append(object_(carrier, 'Dynamic bearing and real wheel carrier', tip,
                        render((.12, .14, .20), (.48, .55, .61)) + assembly_body((.12, .14, .20), 8), parent=root))
    asset = 'section' if fractured else 'arm'
    # The section fixture is 1.5 m; plain arms are 1.2 m. Align root/tip pins
    # exactly with initial material placement, using target-local rest offsets.
    half = .75 if fractured else .6
    center = (tip[0]-half, tip[1], 0)
    rows.append(object_(arm, 'Cohesive material path' if fractured else 'Elastic control' if strong else 'Plastic material path',
                        center, render((half, .13, .18), color)
                        + material(asset, 80, .00001 if strong else .00005,
                                   [('left', root, center), ('right', carrier, (-half, 0, 0))],
                                   plastic=not strong, limit=0 if fractured else 3000 if strong else 180), parent=root))
    fields = wheel(carrier, (0, 0, 0), 1000).replace('wheel.radius .38', 'wheel.radius .30')
    fields = fields.replace('wheel.collisionLayer 0', 'wheel.collisionLayer 1').replace(
        f'wheel.collisionMask {2**64-1}', 'wheel.collisionMask 1')
    fields = fields.replace('wheel.damping 1600', 'wheel.damping 80').replace('wheel.spinInertia 1.2', 'wheel.spinInertia .15')
    rows.append(object_(wheel_id, 'Wheel contact follows bending bearing', (0, -.4, 0),
                        f'  render mesh\n  render.mesh-asset "{IDS["wheel"]}"\n  render.color .06 .07 .09\n'
                        + fields, parent=carrier))
    rows.append(object_(root+4, 'Visible physical axle orientation', (.16, 0, 0),
                        render((.035, .20, .035), (.95, .85, .45)), parent=wheel_id))
    return ''.join(rows)


def crash_car(root, position, strong=False):
    """Two real-carrier assemblies differ only in authored connection capacity.

    The front finite carriers lead the chassis collision box. A car driving
    forward therefore physically strikes its bearing bodies against the wall;
    query tyres are not claimed to be horizontal rigid collision geometry.
    """
    color = (.15, .70, .57) if strong else (.96, .46, .12)
    shafts = [shaft('motor', inertia=.4, drag=.01, bearing=root)]
    links = []
    for index in range(4):
        shafts.append(shaft(f'wheel{index+1}', 'wheel', target=root+1+index))
        row = link(f'drive{index+1}', 'clutch', 0, index+1, .25, 120)
        row['connection'] = connection(root+20+index, 'right' if index % 2 else 'left',
                                        'left' if index % 2 else 'right')
        links.append(row)
    graph = {'version': 1, 'enabled': True, 'shafts': shafts,
             'sources': [source('motor', 0, 'electric', 80, 6000)],
             'links': links, 'brakes': [brake(f'wheel{i+1}', i+1, 100) for i in range(4)]}
    rows = [object_(root, 'Strong crash rover' if strong else 'Weak-mount crash rover',
                    position, render((.72, .20, 1.10), color)
                    + assembly_body((.72, .20, 1.10), 120) + graph_line(graph))]
    for index, (z, side) in enumerate(((-1.35, -1), (-1.35, 1), (1.05, -1), (1.05, 1))):
        carrier, arm = root+10+index, root+20+index
        center, mount = (side*.82, -.05, z), (side*1.42, -.05, z)
        tip, root_group = ('right', 'left') if side > 0 else ('left', 'right')
        rows.append(object_(carrier, 'Protruding finite wheel bearing / collision body', mount,
                            render((.18, .14, .22), (.56, .61, .65))
                            + assembly_body((.18, .14, .22), 8), parent=root))
        fields = material('arm', 80, .0000001,
                          [(root_group, root, center), (tip, carrier, (-side*.6, 0, 0))],
                          plastic=True)
        fields += '  deformable.attachmentLimits ' + json.dumps(
            f'1 "{tip}" {15000 if strong else 1500} 0') + '\n'
        rows.append(object_(arm, 'Plastic suspension material / finite terminal patch', center,
                            render((.6, .13, .18), color) + fields, parent=root))
        fields = wheel(carrier, (0, 0, 0), 2400)
        fields = fields.replace('wheel.collisionLayer 0', 'wheel.collisionLayer 1').replace(
            f'wheel.collisionMask {2**64-1}', 'wheel.collisionMask 1')
        for old, new in (('wheel.radius .38', 'wheel.radius .34'),
                         ('wheel.damping 1600', 'wheel.damping 180'),
                         ('wheel.spinInertia 1.2', 'wheel.spinInertia .3')):
            fields = fields.replace(old, new)
        rows.append(object_(root+1+index, 'Physical rolling contact on actual bearing',
                            (0, -.4, 0), f'  render mesh\n  render.mesh-asset "{IDS["wheel"]}"\n'
                            '  render.color .055 .065 .075\n' + fields, parent=carrier))
        rows.append(object_(root+40+index, 'Visible wheel rotation stripe', (side*.155, 0, 0),
                            render((.025, .045, .26), (.96, .89, .65)), parent=root+1+index))
    # Recognizable low-poly car presentation remains independent of collision
    # and mounting mechanics; none of these meshes prescribes a damaged pose.
    rows += [object_(root+30, 'Cabin body', (0, .47, .20),
                     render((.50, .28, .56), color), parent=root),
             object_(root+31, 'Front windscreen', (0, .55, -.375),
                     render((.44, .20, .035), (.11, .26, .34)), parent=root),
             object_(root+32, 'Rear window', (0, .55, .775),
                     render((.44, .18, .025), (.11, .26, .34)), parent=root),
             object_(root+33, 'Roof', (0, .76, .20),
                     render((.54, .055, .60), (.29, .34, .39)), parent=root),
             object_(root+34, 'Front bumper visual', (0, -.015, -1.14),
                     render((.75, .10, .08), (.27, .31, .35)), parent=root),
             object_(root+35, 'Left headlight', (-.48, .11, -1.115),
                     render((.16, .075, .02), (1, .88, .53)), parent=root),
             object_(root+36, 'Right headlight', (.48, .11, -1.115),
                     render((.16, .075, .02), (1, .88, .53)), parent=root),
             object_(root+37, 'Left side window', (-.515, .53, .22),
                     render((.02, .17, .43), (.11, .26, .34)), parent=root),
             object_(root+38, 'Right side window', (.515, .53, .22),
                     render((.02, .17, .43), (.11, .26, .34)), parent=root)]
    return ''.join(rows)


def create_crash():
    """Add the clear wall-impact experience without rewriting the old fixtures."""
    (OUT/'Scenes').mkdir(parents=True, exist_ok=True)
    (OUT/'Assets/prefabs').mkdir(parents=True, exist_ok=True)
    rows = [object_(1, 'Crash lane uniform gravity', (0, 0, 0),
                    '  gravity uniform 9.81\n  gravity.region sphere 250\n'),
            object_(3, 'Crash lane physical ground', (0, -.20, 3),
                    render((11, .20, 26), (.095, .13, .16)) + body((11, .20, 26))),
            object_(10, 'Project-owned crash driving, view and UI', (0, 0, 0),
                    script({'crashDemo': True}) + f'  ui.asset "{IDS["ui"]}"\n'
                    '  ui.name "lab"\n  ui.enabled true\n'),
            crash_car(900, (-4, .99, 13)), crash_car(1100, (4, .99, 13), True)]
    for identity, x, color in ((1500, -4, (.91, .37, .12)), (1501, 4, (.12, .57, .48))):
        rows.append(object_(identity, 'Ordinary solid impact wall', (x, 1.0, -6),
                            render((2.3, 1.0, .25), (.35, .40, .43)) + body((2.3, 1.0, .25))))
        rows.append(object_(identity+10, 'Lane-color impact marker', (x, 1.75, -5.74),
                            render((2.10, .12, .025), color)))
        for index in range(7):
            stripe=1700+(identity-1500)*40+index*2
            rows.append(object_(stripe, 'Lane edge stripe',
                                (x-1.95, .012, 13-index*3), render((.035, .012, .72), color)))
            rows.append(object_(stripe+1, 'Lane edge stripe',
                                (x+1.95, .012, 13-index*3), render((.035, .012, .72), color)))
    # Raised coloured start bars and clear centre dashes make direction readable.
    for index in range(9):
        rows.append(object_(1600+index, 'Centre lane dash', (0, .014, 15-index*3),
                            render((.055, .014, .60), (.75, .81, .83))))
    rows.append(object_(12, 'Compatibility view seed only', (0, 7, 20),
                        '  player-start first-person\n  player-start.yaw 0\n'))
    (OUT/'Scenes/crash.judas').write_text(settings('Mechanical damage / two-car wall crash') + '\n'.join(rows))
    (OUT/'Assets/prefabs/crash_car.judasprefab').write_text(
        settings('Physical carrier-supported crash rover', 80) + crash_car(1, (0, 0, 0)))
    metadata('Assets/prefabs/crash_car.judasprefab', 'crash_car', 'prefab')


def create():
    for directory in ('Assets/deformables', 'Assets/scripts', 'Assets/models',
                      'Assets/materials', 'Assets/fonts', 'Assets/ui', 'Assets/prefabs',
                      'Assets/worlds', 'Scenes'):
        (OUT / directory).mkdir(parents=True, exist_ok=True)
    for name, args in (
        ('arm', ['block', '4', '1', '1', '1.2', '.26', '.36']),
        ('section', ['flex-structure', '4', '1', '1', '1.5', '.26', '.36', '30000', '30000', '1e-8'])):
        path = f'Assets/deformables/{name}.judasdeform'
        subprocess.run([str(BAKER), args[0], str(OUT/path), *args[1:]], check=True)
        metadata(path, name, 'deformable')
    for name in ('wheel.obj', 'LICENSE.txt'):
        shutil.copyfile(ROOT/'projects/wheel_lab/Assets/models'/name, OUT/'Assets/models'/name)
    for name in ('DejaVuSans.ttf', 'LICENSE.txt'):
        shutil.copyfile(ROOT/'projects/wheel_lab/Assets/fonts'/name, OUT/'Assets/fonts'/name)
    shutil.copyfile(ROOT/'projects/wheel_lab/Assets/scripts/math.js', OUT/'Assets/scripts/math.js')
    for name in ('outer', 'inner'):
        shutil.copyfile(ROOT/f'projects/fracture_lab/Assets/materials/{name}.judasmat', OUT/f'Assets/materials/{name}.judasmat')
        metadata(f'Assets/materials/{name}.judasmat', name, 'material')
    for path, name, kind in (
        ('Assets/models/wheel.obj', 'wheel', 'mesh'), ('Assets/scripts/math.js', 'math', 'script'),
        ('Assets/scripts/controller.js', 'controller', 'script'), ('Assets/fonts/DejaVuSans.ttf', 'font', 'font'),
        ('Assets/ui/lab.judasui', 'ui', 'ui'), ('Assets/prefabs/buggy.judasprefab', 'buggy', 'prefab'),
        ('Assets/prefabs/bench.judasprefab', 'bench', 'prefab'), ('Assets/prefabs/ram.judasprefab', 'ram', 'prefab'),
        ('Assets/worlds/lab.judasworld', 'world', 'world')):
        metadata(path, name, kind)
    ui = (ROOT/'projects/powertrain_lab/Assets/ui/lab.judasui').read_text().replace(
        '82828282828282828282828200000005', IDS['font'])
    replacements = {
        'JUDAS / TORQUE, GEARS & MECHANICAL LOADS': 'JUDAS / COUPLED PHYSICAL DAMAGE',
        'Combustion transmission / independent electric wheels / physical flywheel': 'Plastic mount / strong control / cohesive fracture / powered buggies',
        'W/S throttle | Q/E gear | F clutch | A/D steer | Space brake | Tab machine': 'B load | I physical bearing impact | H overload | Tab assembly | V watch',
        'Finite inertia and mechanical constraints transfer real torque. RPM is measured.': 'Material bends. Real bearings move. Connections fail. Mechanical loads change.',
        'WASD/Space | Q/E gear | F clutch | Tab select | V watch | B load | H grip | P prefab | F3 garage | 1/2/3 | F5/F8 | R | Esc': 'W/S power | A/D steer | Space brake | B load | I impact | H overload | Tab | V watch | P prefab | F3 annex | 1/2 | F5/F8 | R | Esc',
        'POWERTRAIN LAB PAUSED': 'DAMAGE LAB PAUSED', 'powertrain-current': 'damage-current'}
    for old, new in replacements.items():
        ui = ui.replace(old, new)
    (OUT/'Assets/ui/lab.judasui').write_text(ui)
    actions = {'brake': 'key:Space', 'select': 'key:Tab', 'autopilot': 'key:V',
               'load': 'key:B', 'impact': 'key:I', 'overload': 'key:H',
               'spawn': 'key:P', 'region': 'key:F3', 'crash': 'key:1', 'flat': 'key:2', 'oblique': 'key:3',
               'save_game': 'key:F5', 'load_game': 'key:F8', 'pause': 'key:Escape',
               'restart': 'key:R', 'view': 'key:C', 'ui_click': 'mouse:Left',
               'ui_activate': 'key:Return', 'ui_up': 'key:Up', 'ui_down': 'key:Down'}
    axes = {'throttle': [('key:S', -1), ('key:W', 1)], 'steer': [('key:D', -1), ('key:A', 1)],
            'look_x': [('mouse:dx', 1)], 'look_y': [('mouse:dy', 1)]}
    mapping = f'1 {len(actions)+len(axes)} ' + ''.join(
        f'"{name}" 0 1 "{binding}" 1 0 ' for name, binding in actions.items()) + ''.join(
        f'"{name}" 1 {len(bindings)} ' + ''.join(f'"{binding}" {scale} 0 ' for binding, scale in bindings)
        for name, bindings in axes.items())
    classification = 'JudasClassification 1 0 0 2 2 0 "Environment" 1 "Assembly" 1 1 0 "Default"'
    (OUT/'damage_lab.judasproj').write_text(
        'JudasProject 1\nname "Mechanical Damage Lab"\nstartup-scene "Scenes/flat.judas"\n'
        'assets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\nlegacy-gameplay "false"\n'
        'classification ' + json.dumps(classification) + '\ninput-map ' + json.dumps(mapping)
        + f'\nworld-manifest "{IDS["world"]}"\n')
    for mode in ('flat', 'oblique'):
        q = rotation((0, 0, 1), .65) if mode == 'oblique' else (1, 0, 0, 0)
        rows = [object_(1, 'Ordinary selected uniform gravity', (0, 0, 0),
                        '  gravity uniform 9.81\n  gravity.region sphere 250\n', q),
                object_(3, 'Ordinary collision floor', turn(q, (0, -.3, 0)),
                        render((25, .3, 28), (.12, .19, .23)) + body((25, .3, 28)), q),
                object_(10, 'Project-owned loading, driving, camera and UI', (0, 0, 0), script()
                        + f'  ui.asset "{IDS["ui"]}"\n  ui.name "lab"\n  ui.enabled true\n')]
        for identity, x, strong, fractured in ((100, -7, False, False), (200, 0, True, False), (300, 7, False, True)):
            rows.append(bench(identity, turn(q, (x, 1.1, 0)), q, strong, fractured))
        rows += [buggy(500, turn(q, (-5, .94, 12)), q),
                 buggy(700, turn(q, (5, .94, 12)), q, True)]
        for index, x in enumerate((-7, 0, 7)):
            color = ((.96, .52, .14), (.15, .69, .60), (.65, .34, .81))[index]
            rows.append(object_(400+index, 'Station color / comparison marker', turn(q, (x+1, .025, 1.6)),
                                render((1.7, .025, .10), color), q))
        rows.append(object_(12, 'Compatibility camera seed only', turn(q, (-3, 6, 8)),
                            '  player-start first-person\n  player-start.yaw 0\n', q))
        (OUT/'Scenes'/f'{mode}.judas').write_text(settings('Mechanical damage / ' + mode) + '\n'.join(rows))
    (OUT/'Assets/prefabs/buggy.judasprefab').write_text(settings('Coupled material suspension prefab', 70)
        + buggy(1, (0, 0, 0)))
    (OUT/'Assets/prefabs/bench.judasprefab').write_text(settings('Independent coupled bearing prefab', 8)
        + bench(1, (0, 0, 0)))
    (OUT/'Assets/prefabs/ram.judasprefab').write_text(settings('Ordinary finite-mass impact ram', 2)
        + object_(1, 'Physical bearing impact ram', (0, 0, 0), render((.20, .25, .25), (.90, .81, .44))
                  + body((.20, .25, .25), 12)))
    (OUT/'Scenes/annex.judas').write_text(settings('Retained coupled damage specimen', 10)
        + bench(1, (35, 1.1, 0))
        + object_(9, 'Annex floor', (35, -.3, 0), render((5, .3, 5), (.24, .29, .32)) + body((5, .3, 5))))
    (OUT/'Assets/worlds/lab.judasworld').write_text(
        'JudasWorld 1\nbudget 2 8 2 8388608 8388608 8388608 2097152\n'
        'region "annex" "Scenes/annex.judas" 0 0 0 0 0 0 1 40 4 6 1 "snapshot" 262144 ""\n')
    print(OUT)


if __name__ == '__main__':
    if '--crash-only' not in sys.argv:
        create()
    create_crash()

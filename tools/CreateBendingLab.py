#!/usr/bin/env python3
"""Ordinary M80 content. Loads and physical contacts, never scripted deformation.

The existing scene-format helpers are reused without executing the historical
lab generator. Run with a current judas_deformable_bake executable.
"""
from pathlib import Path
import ast
import json
import math
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / 'projects/bending_lab'
baker = Path(sys.argv[1] if len(sys.argv) > 1 else root / 'build/judas_deformable_bake')
for directory in ('Assets/deformables', 'Assets/scripts', 'Assets/materials',
                  'Assets/ui', 'Assets/fonts', 'Assets/prefabs', 'Assets/worlds', 'Scenes'):
    (out / directory).mkdir(parents=True, exist_ok=True)
ids = {name: '80' * 12 + f'{n:08x}' for n, name in enumerate(
    ('elastic', 'plastic', 'panel', 'flex', 'controller', 'lab', 'ui',
     'elastic_prefab', 'plastic_prefab', 'world', 'outer', 'inner', 'font', 'math'), 1)}
tree = ast.parse((root / 'tools/CreateDeformableLab.py').read_text())
exec(compile(ast.Module([n for n in tree.body if isinstance(n, ast.FunctionDef)
                        and n.name in ('v', 'obj', 'render', 'body', 'script', 'deform', 'settings')],
                       type_ignores=[]), '<existing scene format>', 'exec'), globals())


def meta(path, key, kind):
    (out / (path + '.judasmeta')).write_text(
        f'JudasAssetMeta 1\nid "{ids[key]}"\ntype {kind}\nsource ""\n')


for name, args in (
    ('elastic', ['block', '4', '2', '2', '3', '1', '1']),
    ('plastic', ['block', '4', '2', '2', '3', '1', '1']),
    ('panel', ['block', '4', '3', '1', '2', '2', '.35']),
    ('flex', ['flex-structure', '4', '1', '1', '4', '1', '1', '10000', '10000', '1e-6']),
):
    path = f'Assets/deformables/{name}.judasdeform'
    subprocess.run([str(baker), args[0], str(out / path), *args[1:]], check=True)
    meta(path, name, 'deformable')

for name in ('outer', 'inner'):
    path = f'Assets/materials/{name}.judasmat'
    shutil.copyfile(root / 'projects/fracture_lab' / path, out / path)
    meta(path, name, 'material')
for name in ('DejaVuSans.ttf', 'LICENSE.txt'):
    shutil.copyfile(root / 'projects/fracture_lab/Assets/fonts' / name, out / 'Assets/fonts' / name)
meta('Assets/fonts/DejaVuSans.ttf', 'font', 'font')
for name in ('controller', 'math'):
    path = f'Assets/scripts/{name}.js'
    shutil.copyfile(root / 'projects/fracture_lab' / path, out / path)
    meta(path, name, 'script')
controller = out / 'Assets/scripts/controller.js'
controller.write_text(controller.read_text().replace('pitch:-.2', 'pitch:.1'))
for path, key, kind in (
    ('Assets/scripts/lab.js', 'lab', 'script'),
    ('Assets/ui/lab.judasui', 'ui', 'ui'),
    ('Assets/prefabs/elastic.judasprefab', 'elastic_prefab', 'prefab'),
    ('Assets/prefabs/plastic.judasprefab', 'plastic_prefab', 'prefab'),
    ('Assets/worlds/lab.judasworld', 'world', 'world'),
):
    meta(path, key, kind)

ui = (root / 'projects/fracture_lab/Assets/ui/lab.judasui').read_text()
ui = ui.replace('FRACTURE LAB', 'BENDING LAB').replace('fracture-current', 'bending-current')
ui = ui.replace('be36a149b9918581ce406f3632b79cab', ids['font'])
texts = {
    'title': 'JUDAS / ELASTICITY, PLASTICITY & FAILURE',
    'score': 'A elastic | B plastic | C flexible connections | D physical dent',
    'progress': 'B: beam load/unload | H: increasing C load | G/T: carry/throw press',
    'message': 'Load the beams, unload them and compare recovery. Orange press is a real rigid body.',
    'controls': 'WASD/mouse | B beam load | H failure load | Click impact | G/T carry/throw | P prefab | F3 annex | 1/2 scenes | F5/F8 save/load | F9 reset | Esc',
}
lines = ui.splitlines()
for index, line in enumerate(lines):
    for element, text in texts.items():
        if line.startswith('"' + element + '" '):
            end = line.rfind(' "" "')
            begin = line.rfind(' "', 0, end)
            lines[index] = line[:begin] + ' ' + json.dumps(text) + line[end:]
            if element == 'title':
                lines[index] = lines[index].replace('540 36', '1050 36')
            elif element == 'score':
                lines[index] = lines[index].replace('740 30', '1100 30').replace('1 10 22 0 0 1', '1 10 20 0 0 1')
            elif element == 'progress':
                lines[index] = lines[index].replace('850 26', '1120 26')
            elif element == 'controls':
                lines[index] = lines[index].replace('1 10 16 0 0 1', '1 10 14 0 0 1')
(out / 'Assets/ui/lab.judasui').write_text('\n'.join(lines) + '\n')

actions = {'jump': 'key:Space', 'interact': 'key:G', 'throw': 'key:T',
           'load': 'key:B', 'adopt': 'key:H', 'spawn': 'key:P', 'region': 'key:F3',
           'flat': 'key:1', 'radial': 'key:2', 'save_game': 'key:F5', 'load_game': 'key:F8',
           'pause': 'key:Escape', 'restart': 'key:F9', 'push': 'mouse:Left',
           'ui_click': 'mouse:Left', 'ui_activate': 'key:Return', 'ui_up': 'key:Up', 'ui_down': 'key:Down'}
axes = {'move_x': [('key:A', -1), ('key:D', 1)], 'move_y': [('key:S', -1), ('key:W', 1)],
        'look_x': [('mouse:dx', 1)], 'look_y': [('mouse:dy', 1)],
        'look_stick_x': [('stick:RightX', 1)], 'look_stick_y': [('stick:RightY', 1)],
        'wheel_y': [('mouse:wheelY', 1)]}
mapping = f'1 {len(actions) + len(axes)} ' + ''.join(
    f'"{name}" 0 1 "{binding}" 1 0 ' for name, binding in actions.items()) + ''.join(
    f'"{name}" 1 {len(bindings)} ' + ''.join(f'"{binding}" {scale} 0 ' for binding, scale in bindings)
    for name, bindings in axes.items())
classification = 'JudasClassification 1 1 1 0 "pickup" 1 1 0 "Default" 1 1 0 "Default"'
(out / 'bending_lab.judasproj').write_text(
    'JudasProject 1\nname "Bending Lab"\nstartup-scene "Scenes/flat.judas"\n'
    'assets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\nlegacy-gameplay "false"\n'
    + 'classification ' + json.dumps(classification) + '\ninput-map ' + json.dumps(mapping)
    + '\nworld-manifest "' + ids['world'] + '"\n')


def specimen(i, name, position, asset, color, q=(1, 0, 0, 0), plastic=False, panel=False):
    pins = [('left', 0, 0, '', (0, 0, 0))]
    if panel:
        pins.append(('right', 0, 0, '', (0, 0, 0)))
    half = (1, 1, .175) if panel else ((2, .5, .5) if asset == 'flex' else (1.5, .5, .5))
    density = 1 if asset in ('elastic', 'plastic') else 10
    fields = render(half, color) + deform(asset, density=density, shear=.0005,
                                        volume=.000005, attachments=pins, plastic=plastic)
    fields = fields.replace('  deformable.material.damping "0.7"', '  deformable.material.damping "1"')
    yield_strain = .04
    fields = fields.replace('  deformable.material.yieldStrain "0.06"',
                            '  deformable.material.yieldStrain "' + str(yield_strain) + '"')
    result = obj(i, name, position, fields, q)
    return result.replace(json.dumps('1 "62626262626262626262626262629997" 0'),
                          json.dumps('2 "' + ids['outer'] + '" 0 "' + ids['inner'] + '" 0'))


for radial in (False, True):
    rows = [obj(1, 'Actual gravity field', (0, -100, 0) if radial else (0, 0, 0),
                '  gravity ' + ('radial' if radial else 'uniform') + ' 9.81\n  gravity.region sphere 400\n')]
    rows += [obj(3, 'Laboratory floor', (9, -.3, 0), render((23, .3, 22), (.13, .19, .23)) + body((23, .3, 22))),
             obj(10, 'Script-controlled visitor', (9, 1.1, 14), '  motor.enabled true\n  motor.radius .3\n  motor.halfHeight .6\n'
                 + script('controller') + f'  ui.asset "{ids["ui"]}"\n  ui.name "lab"\n  ui.enabled true\n'),
             obj(11, 'Ordinary load controls', (0, 0, 0), script('lab')),
             obj(12, 'Compatibility view seed', (9, 1.1, 14), '  player-start first-person\n')]
    for i, (x, asset, label, color, plastic) in enumerate((
        (0, 'elastic', 'A: elastic cantilever / load and recover', (.10, .78, .60), False),
        (6, 'plastic', 'B: plastic cantilever / permanent rest strain', (.94, .48, .10), True),
        (12, 'flex', 'C: flexible structure / real section failure', (.18, .60, .94), False),
    )):
        rows.append(specimen(100 + i * 100, label, (x, 3, 0), asset, color, plastic=plastic))
        edge = -2 if asset == 'flex' else -1.5
        rows.append(obj(500 + i, 'Physical clamped support', (x + edge - .18, 1.65, 0),
                        render((.18, 1.65, .8), (.30, .36, .42)) + body((.18, 1.65, .8))))
        rows.append(obj(600 + i, 'Station color marker', (x, .12, -2), render((1.3, .12, .18), color)))
    q = (math.sqrt(.5), math.sqrt(.5), 0, 0)
    rows.append(specimen(400, 'D: yielding panel / actual rigid contact', (18, 1.6, 0), 'panel', (.72, .30, .82), q, True, True))
    for side in (-1, 1):
        rows.append(obj(510 + (side + 1) // 2, 'Panel clamped edge', (18 + side * 1.1, .8, 0),
                        render((.1, .8, 1.1), (.32, .38, .43)) + body((.1, .8, 1.1))))
    rows.append(obj(410, 'Movable finite-mass indentation press', (18, 2.9, 0),
                    render((.32, .22, .32), (.95, .50, .16)) + body((.32, .22, .32), True, 2) + '  tags 1\n'))
    rows.append(obj(411, 'Throwing / collision proof prop', (9, .5, 4),
                    render((.35, .35, .35), (.72, .76, .82)) + body((.35, .35, .35), True, 2) + '  tags 1\n'))
    oblique = (math.cos(.35), 0, 0, math.sin(.35))
    rows.insert(0, obj(5, 'Oblique ordinary uniform region', (-7, 4, -5),
                       '  gravity uniform 9.81\n  gravity.region box 3 6 3\n', oblique))
    rows.append(specimen(450, 'E: oblique elastic specimen', (-7, 4, -5), 'elastic', (.38, .75, .82), oblique))
    (out / 'Scenes' / ('radial.judas' if radial else 'flat.judas')).write_text(
        settings('Bending lab / ' + ('RadicalGravity' if radial else 'FaithfulGravity')) + '\n'.join(rows))

for key, plastic in (('elastic', False), ('plastic', True)):
    (out / f'Assets/prefabs/{key}.judasprefab').write_text(
        settings('Independent ' + key + ' specimen', 2)
        + specimen(1, key + ' beam', (0, 0, 0), key, (.25, .70, .55), plastic=plastic))
(out / 'Scenes/annex.judas').write_text(settings('Streaming yielding structure', 3)
    + specimen(1, 'Streamed plastic beam', (35, 3, 0), 'plastic', (.90, .50, .18), plastic=True)
    + obj(2, 'Annex floor', (35, -.3, 0), render((5, .3, 4), (.20, .25, .29)) + body((5, .3, 4))))
(out / 'Assets/worlds/lab.judasworld').write_text(
    'JudasWorld 1\nbudget 2 8 2 8388608 8388608 8388608 2097152\n'
    'region "annex" "Scenes/annex.judas" 0 0 0 0 0 0 1 40 7 5 1 "snapshot" 262144 ""\n')
print(out)

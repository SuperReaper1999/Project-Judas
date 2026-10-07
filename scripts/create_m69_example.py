#!/usr/bin/env python3
"""Create a small ordinary M69 input/query example in a disposable project copy.

This writes content only. It does not compile, run tests, launch Judas, export a
package or modify the source project. Existing output is deliberately preserved.
"""
import argparse
import json
from pathlib import Path
import re
import shlex
import shutil

ROOT = Path(__file__).resolve().parents[1]
PREFIX = '69' * 12
FONT = '41414141414141414141414141414103'


def asset(number):
    return PREFIX + f'{number:08x}'


def write_asset(path, text, number, kind):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8')
    Path(str(path) + '.judasmeta').write_text(
        f'JudasAssetMeta 1\nid "{asset(number)}"\ntype {kind}\nsource ""\n')


def paired_map(legacy):
    """Retain the copied map, adding only the ordinary example bindings."""
    tokens = shlex.split(legacy)
    if tokens[0] != '1':
        raise ValueError('The source fixture must use the existing version-1 map')
    count, cursor, entries = int(tokens[1]), 2, []
    for _ in range(count):
        name, kind, length = tokens[cursor:cursor + 3]
        cursor += 3
        bindings = []
        for _ in range(int(length)):
            control, scale, deadzone = tokens[cursor:cursor + 3]
            cursor += 3
            bindings.append([control, scale, deadzone, '1', '0'])
        entries.append([name, kind, bindings])
    if cursor != len(tokens):
        raise ValueError('Unexpected trailing source input-map data')
    entries += [['flick_stick', '2', [['stick:Right', '1', '0.2', '1', '1']]],
                ['compare_rays', '0', [['key:C', '1', '0', '1', '0']]],
                ['fixture_quit', '0', [['key:Q', '1', '0', '1', '0']]]]
    output = ['2', str(len(entries))]
    for name, kind, bindings in entries:
        output += [json.dumps(name), kind, str(len(bindings))]
        for control, scale, deadzone, scale_y, circular in bindings:
            output += [json.dumps(control), scale, deadzone, scale_y, circular]
    return ' '.join(output)


def ui_element(identity, parent='', kind=2, position=(0, 0), size=(180, 40), text='', **patch):
    # The complete normal M67 schema is intentionally ordinary project content.
    element = dict(id=identity, parent=parent, text=text, texture='', font=FONT,
                   textKey='', textLogicalAlign=-1, mirrorRow=False,
                   visible=True, enabled=True, clip=False, wrap=True, fit=True,
                   spacing=8, fontSize=20, value=0, minimum=0, maximum=1,
                   kind=kind, flow=0, direction=0, anchorMin=[0, 0], anchorMax=[0, 0],
                   offset=list(position), size=list(size), relativeSize=[0, 0],
                   align=[0, 0], textAlign=[0, 0], margin=[0, 0, 0, 0],
                   padding=[0, 0, 0, 0], background=[0, 0, 0, 0], color=[.92, .96, 1, 1])
    element.update(patch)
    return element


def hud_document():
    elements = [ui_element('canvas', kind=0, size=(0, 0)),
                ui_element('title', 'canvas', position=(24, 20), size=(1232, 48),
                           text='M69 / PAIRED INPUT + 100-RAY QUERIES', fontSize=27),
                ui_element('input_panel', 'canvas', kind=1, position=(24, 84), size=(604, 552),
                           clip=True, background=[.025, .06, .09, .92]),
                ui_element('raw', 'input_panel', position=(14, 18), size=(576, 44), text='Raw: neutral'),
                ui_element('circular', 'input_panel', position=(14, 62), size=(576, 44), text='Circular: neutral'),
                ui_element('delta', 'input_panel', position=(14, 106), size=(576, 64), text='Frame delta: neutral'),
                ui_element('samples', 'input_panel', position=(14, 182), size=(576, 240), text='Ordered backend observations: none', fontSize=18),
                ui_element('flags', 'input_panel', position=(14, 430), size=(576, 100), text='Waiting for fixed boundary', fontSize=18),
                ui_element('query_panel', 'canvas', kind=1, position=(650, 84), size=(606, 552),
                           clip=True, background=[.035, .065, .09, .92]),
                ui_element('fan', 'query_panel', position=(14, 18), size=(578, 110), text='100-ray fan: waiting'),
                ui_element('ordered', 'query_panel', position=(14, 134), size=(578, 210), text='Results keep input order', fontSize=18),
                ui_element('comparison', 'query_panel', position=(14, 350), size=(578, 100), text='Press C to compare all 100 with scalar rays'),
                ui_element('limits', 'query_panel', position=(14, 458), size=(578, 80), fontSize=17,
                           text='One batch bridge / 100 geometric queries.\nNo ledge selection or trick recognizer.'),
                ui_element('help', 'canvas', position=(24, 654), size=(1232, 56), fontSize=18,
                           text='Right stick: diagonal and out/back samples | C: scalar compare | Esc: pause | R: reload | Q: quit\nNo controller means neutral input. Hardware feel still needs human review.'),
                ui_element('pause', 'canvas', kind=1, position=(400, 210), size=(480, 300),
                           visible=False, clip=True, background=[.015, .025, .05, 1]),
                ui_element('pause_title', 'pause', position=(20, 18), size=(440, 52), text='PAUSED / history reset', fontSize=25),
                ui_element('resume', 'pause', kind=4, position=(20, 86), size=(440, 54), text='Resume', background=[.1, .24, .35, 1]),
                ui_element('reload', 'pause', kind=4, position=(20, 154), size=(440, 54), text='Reload example', background=[.1, .24, .35, 1]),
                ui_element('quit', 'pause', kind=4, position=(20, 222), size=(440, 54), text='Quit', background=[.1, .24, .35, 1])]
    return {'kind': 'ui', 'schema': 1, 'data': {'reference': [1280, 720],
            'visible': True, 'enabled': True, 'modal': False, 'elements': elements}}


PAIRED_DISPLAY = r'''import Reader from './paired-stick.js';
import {ui} from 'judas';
const pair=v=>`(${v.x.toFixed(4)}, ${v.y.toFixed(4)})`;
export default class extends Reader {
 update(){super.update();const h=ui.get('m69');if(!h)return;
  const s=this.state;
  h.get('raw').text=`RAW right: ${pair(s.raw)}`;
  h.get('circular').text=`CIRCULAR (inner .20): ${pair(s.circular)}`;
  h.get('delta').text=`Latest render-pump net delta: ${pair(s.frameDelta)}\nOut-and-back may have zero net delta.`;
  h.get('samples').text='Recent ordered observations (seconds):\n'+(s.samples.slice(-6).map(v=>`#${v.sequence}  t=${v.time.toFixed(3)}  ${pair(v)}`).join('\n')||'none / neutral');
  h.get('flags').text=`Cursor ${s.sequence} | capacity ${s.capacity}/stick\nObserved ${s.observations} | resets ${s.resets} | overflows ${s.overflows}\nFrame reads ${s.frameReads} | fixed reads ${s.fixedReads}`;
 }
}
'''

FAN_DISPLAY = r'''import Fan from './ray-fan.js';
export {properties} from './ray-fan.js';
import {ui} from 'judas';
export default class extends Fan {
 update(){const h=ui.get('m69');if(!h)return;const s=this.state;
  h.get('fan').text=`Ordered fan: ${s.count} rays\nHits ${s.hits} | misses ${s.count-s.hits}`;
  h.get('ordered').text='First 6 requests (index preserved):\n'+s.results.slice(0,6).map((v,i)=>v?`${i}: entity ${v.entity} / ${v.distance.toFixed(3)} m`:`${i}: null`).join('\n');
  h.get('comparison').text=s.comparisons?`Scalar comparisons: ${s.comparisons}\nAll 100 equal: ${s.matched?'YES':'NO'}`:'Press C to compare all 100 with scalar rays';
 }
}
'''

VIEW = r'''import {input,ui,world,scenes} from 'judas';
export default class {
 start(){ui.debugOverlayVisible=false;input.pointerCapture=false;
  // Explicit authored flat-fixture view; not an engine world-up convention.
  world.setView({position:{x:0,y:3,z:8},rotation:{w:1,x:0,y:0,z:0}},70);
 }
 uiUpdate(){const h=ui.get('m69');if(!h)return;
  if(input.pressed('pause')){h.modal=!h.modal;h.get('pause').visible=h.modal;}
  if(input.pressed('reset'))scenes.reload();
  if(input.pressed('fixture_quit'))ui.quit();
 }
 onUI(e){if(e.document!=='m69')return;const h=ui.get('m69');if(!h)return;
  // Escape is already handled once in uiUpdate, including the opening frame.
  if(e.type==='click'&&e.element==='resume'){h.modal=false;h.get('pause').visible=false;}
  if(e.type==='click'&&e.element==='reload')scenes.reload();
  if(e.type==='click'&&e.element==='quit')ui.quit();
 }
 destroy(){world.clearView();}
}
'''


def project_scene(source):
    # Preserve copied arena geometry; replace only example-owned script/UI policy.
    source = re.sub(r'^  (?:scripts|script\.[^ ]+|ui\.[^ ]+) .*\n', '', source, flags=re.M)
    match = re.search(r'^object 40 .*?^end$', source, re.M | re.S)
    if not match:
        raise ValueError('Copied character fixture lost its example owner (40)')
    owner = match.group(0)
    owner = re.sub(r'^  body(?:\.[^ ]+)? .*\n', '', owner, flags=re.M)
    owner = re.sub(r'^  position .*$', '  position 0 0 6', owner, flags=re.M)
    owner = re.sub(r'^  rotation .*$', '  rotation 1 0 0 0', owner, flags=re.M)
    slots = ['  scripts 3']
    for index, number in enumerate([3, 4, 5]):
        slots += [f'  script.{index}.id {index+1}', f'  script.{index}.asset "{asset(number)}"',
                  f'  script.{index}.enabled true', f'  script.{index}.properties "{{}}"']
    slots += [f'  ui.asset "{asset(6)}"', '  ui.name "m69"', '  ui.enabled true']
    owner = owner[:-3] + '\n'.join(slots) + '\nend'
    source = source[:match.start()] + owner + source[match.end():]
    source = re.sub(r'^  next-id .*$', '  next-id 70', source, flags=re.M)
    floor = re.search(r'^object 1 .*?^end$', source, re.M | re.S)
    if not floor:
        raise ValueError('Copied character fixture lost its complete box definition')
    # Reuse a complete normal serialized collider/render block, including the
    # existing required fields. This is content construction, not a new format.
    wall = re.sub(r'^object 1 .*$', 'object 69 "Example query wall"', floor.group(0), flags=re.M)
    wall = re.sub(r'^  position .*$', '  position 0 2 -8', wall, flags=re.M)
    wall = re.sub(r'^  (render|body)\.half-extents .*$', r'  \1.half-extents 3.2 2 0.25', wall, flags=re.M)
    wall = re.sub(r'^  render.color .*$', '  render.color 0.1 0.65 0.8', wall, flags=re.M)
    source += '\n' + wall + '\n'
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / '.cache/m69/example')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        parser.error('Output already exists; preserve it and choose a fresh --output.')
    if ROOT / '.cache' not in output.parents and Path('/tmp') not in output.parents:
        parser.error('Use a disposable .cache or /tmp directory, not a current source project.')
    shutil.copytree(ROOT / 'projects/character_demo', output)
    scripts = output / 'Assets/scripts'
    for number, name in [(1, 'paired-stick'), (2, 'ray-fan')]:
        write_asset(scripts / (name + '.js'),
                    (ROOT / f'docs/judasjs/examples/{name}.js').read_text(), number, 'script')
    for number, name, text in [(3, 'paired-display', PAIRED_DISPLAY),
                               (4, 'fan-display', FAN_DISPLAY), (5, 'example-view', VIEW)]:
        write_asset(scripts / (name + '.js'), text, number, 'script')
    write_asset(output / 'Assets/ui/m69.judasui', json.dumps(hud_document(), indent=2) + '\n', 6, 'ui')
    scene = output / 'Scenes/flat.judas'
    scene.write_text(project_scene(scene.read_text()))
    old = output / 'character_demo.judasproj'
    lines = old.read_text().splitlines()
    project = []
    for line in lines:
        if line.startswith('name '):
            line = 'name "M69 Input and Query Examples"'
        elif line.startswith('legacy-gameplay '):
            continue
        elif line.startswith('input-map '):
            line = 'input-map ' + json.dumps(paired_map(shlex.split(line)[1]))
        project.append(line)
    project.append('legacy-gameplay "false"')
    destination = output / 'm69_example.judasproj'
    destination.write_text('\n'.join(project) + '\n')
    old.unlink()
    (output / 'README.md').write_text('''# M69 ordinary input/query example

Generated private copy of character_demo. Current source projects were not edited.
Right stick: cardinal, diagonal, out-and-back readings and ordered observations.
C: compare one 100-ray batch with 100 scalar rays on the same world.
Escape: pause/resume; R: scene reload; Q: quit. Pause also has ordinary UI buttons.
No controller yields neutral input; this fixture cannot certify hardware feel.
No trick recognition, ledge policy or character-controller feature is included.
''')
    print(destination)


if __name__ == '__main__':
    main()

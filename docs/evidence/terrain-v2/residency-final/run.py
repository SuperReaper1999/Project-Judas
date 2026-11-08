from pathlib import Path
import hashlib
import json
import math
import os
import statistics
import subprocess
import time

root = next(p for p in Path(__file__).resolve().parents if (p / 'CMakeLists.txt').is_file())
out = Path(__file__).resolve().parent
build = root / 'build'
env = dict(os.environ, SDL_AUDIODRIVER='dummy')
rows = []

def run(name, command, additions=None):
    start = time.monotonic()
    with (out / (name + '.log')).open('w') as log:
        result = subprocess.run(list(map(str, command)), cwd=root,
                                env=dict(env, **(additions or {})),
                                stdout=log, stderr=subprocess.STDOUT)
    rows.append(dict(name=name, command=list(map(str, command)), exit=result.returncode,
                     seconds=time.monotonic()-start))
    (out / 'final-runs.json').write_text(json.dumps(rows, indent=2)+'\n')
    if result.returncode:
        raise RuntimeError(name + ' failed; inspect its log')

for profile in ['modern', 'portable']:
    run('qualification-' + profile + '-confirmed',
        ['python3', root/'scripts/prealpha_validate.py', '--build-dir', build,
         '--group', 'terrain', '--test', 'renderer-shader-library',
         '--graphics-profile', profile,
         '--output', out/('qualification-' + profile + '-confirmed')], dict(JUDAS_GL_PROFILE=profile))

for profile in ['modern', 'portable']:
    receipt = json.loads((out/('qualification-' + profile + '-confirmed')/'RESULTS.json').read_text())
    assert receipt['passed'] and receipt['graphicsProfile'] == profile
    actual = []
    for log in (out/('qualification-' + profile + '-confirmed')).glob('*.log'):
        for line in log.read_text().splitlines():
            if '[Graphics] API=' in line:
                assert 'requested=' + profile in line and 'selected=' + profile in line, (log, line)
                actual.append(dict(log=log.name, graphics=line))
    assert actual, 'no actual graphics contexts observed'
    (out/('actual-' + profile + '-contexts.json')).write_text(json.dumps(actual, indent=2)+'\n')

for profile in ['modern', 'portable']:
    run('editor-' + profile,
        [build/'judas_terrain_v2_editor_tests',
         '/home/conner/Documents/JudasTerrainV2Acceptance-23dbefde/editor-' + profile],
        dict(JUDAS_GL_PROFILE=profile))
run('residency-cold-read', [build/'judas_terrain_v2_streaming_tests', '--read',
    out/'qualification-modern-confirmed/terrain-v2-residency'])

project = root/'.cache/terrain-v2/residency/kilometre-editable/terrain_v2_workshop.judasproj'
binary = build/'judas_terrain_v2_game_benchmark'
measurements = []
for case in ['short-0', 'short-1', 'short-2', 'traversal']:
    argv = [str(binary), str(project)] + (['--traverse'] if case == 'traversal' else [])
    with (out/('final-'+case+'.jsonl')).open('w') as log, (out/('final-'+case+'.stderr')).open('w') as err:
        result = subprocess.run(['/usr/bin/time', '-v', '-o',
            str(out/('final-'+case+'.rss'))] + argv,
            cwd=root, env=env, stdout=log, stderr=err)
    if result.returncode:
        raise RuntimeError(case + ' failed; inspect stderr')
    data = []
    for line in (out/('final-'+case+'.jsonl')).read_text().splitlines():
        if line.startswith('{'):
            data.append(json.loads(line))
    def dist(values):
        ordered = sorted(values)
        return dict(median=statistics.median(ordered),
                    p95=ordered[math.ceil(len(ordered)*.95)-1],
                    p99=ordered[math.ceil(len(ordered)*.99)-1], worst=ordered[-1])
    phases = {}
    for phase in sorted(set(r['phase'] for r in data if 'phase' in r)):
        records = [r for r in data if r.get('phase') == phase]
        values = {'samples':len(records)}
        for key in records[0]:
            if key.endswith('_ms'):
                values[key] = dist([r[key] for r in records])
            elif key.endswith(('_bytes', '_pages', '_evictions')):
                values[key] = dict(minimum=min(r[key] for r in records),
                                   maximum=max(r[key] for r in records),
                                   end=records[-1][key])
        if phase == 'traversal':
            values['lap_ends'] = [records[(i+1)*8192-1] for i in range(3)]
        phases[phase] = values
    rss = next(int(line.split(':')[1]) for line in (out/('final-'+case+'.rss')).read_text().splitlines()
               if 'Maximum resident set size' in line)
    measurements.append(dict(case=case, command=argv, exit=0, peak_process_rss_kib=rss,
        scene_build_ms=data[0]['scene_build_ms'], phases=phases))
    result = dict(source_revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
                  binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  measurements=measurements)
    (out/'final-measurements.json').write_text(json.dumps(result,indent=2)+'\n')
    print(case, rss, data[0]['scene_build_ms'], flush=True)

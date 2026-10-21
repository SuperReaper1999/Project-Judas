#!/usr/bin/env python3
"""Paired real-slot/fresh-process cold-support probe. No work without --execute."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def vector_distance(left, right):
    return math.sqrt(sum((float(x) - float(y)) ** 2 for x, y in zip(left, right)))


def comparisons(directory):
    read = lambda name: json.loads((directory / name).read_text())
    a, b, repeated = read('uninterrupted.json'), read('restored.json'), read('restored-repeat.json')
    if len(a) != len(b) or len(b) != len(repeated):
        raise RuntimeError('continuation observation row count differs')
    rows = []
    for original, restored in zip(a, b):
        if original['step'] != restored['step']:
            raise RuntimeError('continuation step identities differ')
        metrics = {'step': original['step'], 'max_position_delta_m': 0., 'max_linear_delta_m_s': 0.,
                   'max_angular_delta_rad_s': 0., 'max_quaternion_component_delta': 0.}
        for left, right in zip(original['bodies'], restored['bodies']):
            if left['entity'] != right['entity']:
                raise RuntimeError('restored physical entity identity differs')
            for field, key in [('position', 'max_position_delta_m'), ('linear', 'max_linear_delta_m_s'),
                               ('angular', 'max_angular_delta_rad_s')]:
                metrics[key] = max(metrics[key], vector_distance(left[field], right[field]))
            metrics['max_quaternion_component_delta'] = max(metrics['max_quaternion_component_delta'],
                *(abs(x - y) for x, y in zip(left['rotation'], right['rotation'])))
        rows.append(metrics)
    return {'exact_captured_boundary': a[0] == b[0], 'independent_cold_process_repeat_exact': b == repeated,
            'rows': rows, 'exact_uninterrupted_trajectory': a == b,
            'qualification': 'Post-boundary differences are diagnostic, not an assumed universal exact-save contract.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, default=HERE / 'baseline/frozen/libjudas_engine.a',
                        help='Explicit frozen reference archive; never the mutable current build by default')
    parser.add_argument('--control', type=Path, default=ROOT / 'build/libjudas_engine.a',
                        help='Explicit candidate archive; its exact bytes are recorded')
    parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    output, control, baseline = args.output.resolve(), args.control.resolve(), args.baseline.resolve()
    build = ROOT / 'build'
    target = 'judas_world_persistence_tests'
    flags = {}
    for line in (build / f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    # This private main deliberately accepts arguments; do not inherit a
    # no-argument validation wrapper from the original test driver.
    defines = [flag for flag in flags['CXX_DEFINES'] if not flag.startswith(('-Dmain=', '-DJUDAS_VALIDATION_'))]
    source, obj = HERE / 'ColdSupportSnapshot.cpp', output / 'ColdSupportSnapshot.cpp.o'
    compile_command = ['/usr/bin/c++', *defines, *flags['CXX_INCLUDES'],
                       *flags['CXX_FLAGS'], '-c', str(source), '-o', str(obj)]
    plans = []
    for label, core in [('baseline', baseline), ('control', control)]:
        tokens = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
        tokens = [token for token in tokens if not token.startswith('-Wl,--dependency-file=') and
                  not token.endswith('/tests/ValidationMain.cpp.o')]
        if sum(token.endswith('/tests/WorldPersistenceTests.cpp.o') for token in tokens) != 1:
            raise RuntimeError('unexpected source recipe; inspect before substituting')
        tokens = [str(obj) if token.endswith('/tests/WorldPersistenceTests.cpp.o') else
                  str(core) if token == 'libjudas_engine.a' else token for token in tokens]
        binary = output / label
        tokens[tokens.index('-o') + 1] = str(binary)
        plans.append({'name': label, 'core': str(core), 'core_sha256': sha(core), 'link': tokens,
                      'protocol': [[str(binary), str(output / (label + '-slots')), mode, case]
                                   for case in ['calm', 'impacted']
                                   for mode in ['write', 'read', 'read-repeat', 'missing-contact', 'truncated-contact']]})
    report = {'schema': 'judas.generic-public-cold-support-probe.v1', 'source': str(source),
              'source_sha256': sha(source), 'compile': compile_command, 'plans': plans, 'results': [],
              'notes': ['No contact warm cache is read, injected or serialized by this probe.',
                        'WorldPersistence, GameSnapshot and SaveStorage perform actual public capture/restoration.',
                        'Every reader is a new process; negative controls mutate only in-memory participant copies.',
                        'No generic exact trajectory promise is inferred from narrowly qualified M70/M77 contracts.']}
    if not args.execute:
        print(json.dumps(report, indent=2))
        return
    if output.exists():
        raise RuntimeError('fresh output required')
    output.mkdir(parents=True)
    with (output / 'compile.log').open('w') as stream:
        compiled = subprocess.run(compile_command, cwd=build, stdout=stream, stderr=subprocess.STDOUT)
    report['compile_exit'] = compiled.returncode
    if compiled.returncode == 0:
        for plan in plans:
            with (output / (plan['name'] + '.link.log')).open('w') as stream:
                linked = subprocess.run(plan['link'], cwd=build, stdout=stream, stderr=subprocess.STDOUT)
            record = {'name': plan['name'], 'link_exit': linked.returncode, 'protocol': []}
            if linked.returncode == 0:
                record['binary_sha256'] = sha(output / plan['name'])
                for index, command in enumerate(plan['protocol']):
                    with (output / (plan['name'] + '-' + str(index) + '.log')).open('w') as stream:
                        completed = subprocess.run(command, cwd=ROOT, stdout=stream,
                                                   stderr=subprocess.STDOUT, timeout=120)
                    record['protocol'].append({'argv': command, 'exit': completed.returncode})
                    if completed.returncode:
                        break
                if len(record['protocol']) == len(plan['protocol']) and all(row['exit'] == 0 for row in record['protocol']):
                    record['comparisons'] = {case: comparisons(output / (plan['name'] + '-slots') / case)
                                             for case in ['calm', 'impacted']}
            report['results'].append(record)
            (output / 'RESULTS.json').write_text(json.dumps(report, indent=2) + '\n')
    report['cores_unchanged'] = all(sha(Path(plan['core'])) == plan['core_sha256'] for plan in plans)
    report['source_unchanged'] = sha(source) == report['source_sha256']
    report['positive_and_negative_protocols_passed'] = len(report['results']) == 2 and all(
        len(row.get('protocol', [])) == 10 and all(step['exit'] == 0 for step in row['protocol']) and
        all(case['exact_captured_boundary'] and case['independent_cold_process_repeat_exact']
            for case in row.get('comparisons', {}).values()) for row in report['results'])
    (output / 'RESULTS.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    raise SystemExit(0 if report['positive_and_negative_protocols_passed'] and
                     report['cores_unchanged'] and report['source_unchanged'] else 1)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Replay preserved diagnostic cases using an explicitly selected Linux engine build.

This records current behaviour, including a known scaling defect. It is not an
acceptance test requiring that future solvers preserve the defective behaviour.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
EXPECTED_CORE = 'c724dbbba56538f0fe3b5017dab12f3bb928facdb9b0add21d77297964698eae'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, core, output = args.build.resolve(), args.core.resolve(), args.output.resolve()
    if output.exists():
        parser.error('Choose a new output directory; preserved results are never overwritten.')
    flags_file = build / 'CMakeFiles/judas_engine.dir/flags.make'
    link_file = build / 'CMakeFiles/judas_impact_solver_tests.dir/link.txt'
    if not flags_file.is_file() or not link_file.is_file() or not core.is_file():
        parser.error('Provide a GNU Makefiles Release build with judas_impact_solver_tests built, and its explicit engine archive.')
    output.mkdir(parents=True)
    flags = {}
    for line in flags_file.read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    commands = []
    for name in ['ResponseReplay', 'ProjectionControl']:
        command = ['/usr/bin/c++', *flags['CXX_DEFINES'], '-I' + str(HERE / 'replay'),
                   *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'], '-c',
                   str(HERE / 'replay' / (name + '.cpp')), '-o', str(output / (name + '.o'))]
        # CaseIO's JSON include is relative to the repository root. Recover it
        # from this evidence directory instead of requiring a particular cwd.
        command.insert(1, '-I' + str(HERE.parents[2]))
        commands.append(command)
        subprocess.run(command, check=True)
        link = shlex.split(link_file.read_text())
        link = [part for part in link if not part.startswith('-Wl,--dependency-file=')
                and not part.startswith('CMakeFiles/')]
        link.insert(1, str(output / (name + '.o')))
        link[link.index('-o') + 1] = str(output / name)
        link = [str(core) if part == 'libjudas_engine.a' else part for part in link]
        commands.append(link)
        subprocess.run(link, cwd=build, check=True)
    cases = []
    for compressed in sorted((HERE / 'cases').glob('event-*.json.gz')):
        target = output / compressed.name.removesuffix('.gz')
        target.write_bytes(gzip.decompress(compressed.read_bytes()))
        cases.append(str(target))
    if len(cases) != 10:
        raise RuntimeError('Expected all ten preserved cases.')
    results = {}
    for name, arguments in [('ResponseReplay', cases), ('ProjectionControl', [])]:
        command = [str(output / name), *arguments]
        commands.append(command)
        run = subprocess.run(command, capture_output=True, text=True)
        (output / (name + '.json')).write_text(run.stdout)
        (output / (name + '.stderr')).write_text(run.stderr)
        results[name] = run.returncode
    receipt = {'schema': 'judas.impact-response-reproduction.v1', 'core_sha256': sha(core),
               'same_core_as_original_capture': sha(core) == EXPECTED_CORE,
               'commands': commands, 'exit_codes': results,
               'note': 'A different engine core is a new observation, not verification of the originally measured binary.'}
    (output / 'REPRODUCED.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))
    return int(any(results.values()))


if __name__ == '__main__':
    raise SystemExit(main())

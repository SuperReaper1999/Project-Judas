#!/usr/bin/env python3
"""Replay frozen old event states against the current native impact kernels.

No engine/game edits and no baseline executable are required. Build the Release
impact-solver target first. Outputs go to an explicit disposable directory.
"""
from pathlib import Path
import argparse
import gzip
import hashlib
import json
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    manifest = HERE / 'MANIFEST.json'
    if not manifest.is_file():
        raise SystemExit('Missing evidence manifest; do not replay unverified fixtures.')
    data = json.loads(manifest.read_text())
    for item in data['files']:
        if item['path'].startswith('causal/') or item['path'] == 'replay.py':
            file = HERE / item['path']
            if hashlib.sha256(file.read_bytes()).hexdigest() != item['sha256']:
                raise SystemExit(f'Changed evidence input: {file}')
    flags_file = build / 'CMakeFiles/judas_engine.dir/flags.make'
    link_file = build / 'CMakeFiles/judas_impact_solver_tests.dir/link.txt'
    if not flags_file.is_file() or not link_file.is_file():
        raise SystemExit('Configure with Unix Makefiles and build judas_impact_solver_tests first.')
    flags = {}
    for line in flags_file.read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    output.mkdir(parents=True, exist_ok=False)
    obj, binary = output / 'Replay.o', output / 'Replay'
    commands = [[
        'c++', *flags['CXX_DEFINES'], '-I' + str(ROOT),
        '-I' + str(HERE / 'causal'), *flags['CXX_INCLUDES'],
        *flags['CXX_FLAGS'], '-c', str(HERE / 'causal/ReplayAssertions.cpp'), '-o', str(obj),
    ]]
    subprocess.run(commands[0], cwd=ROOT, check=True)
    link = [x for x in shlex.split(link_file.read_text())
            if not x.startswith(('CMakeFiles/', '-Wl,--dependency-file='))]
    link.insert(1, str(obj))
    link[link.index('-o') + 1] = str(binary)
    commands.append(link)
    subprocess.run(link, cwd=build, check=True)
    cases = []
    for source in sorted((HERE / 'causal/cases').glob('event-*.json.gz')):
        target = output / source.name.removesuffix('.gz')
        target.write_bytes(gzip.decompress(source.read_bytes()))
        cases.append(str(target))
    if len(cases) != 10:
        raise SystemExit('Expected all ten preserved original events.')
    commands.append([str(binary), *cases])
    with (output / 'RESULTS.json').open('w') as stream:
        status = subprocess.run(commands[-1], cwd=ROOT, stdout=stream).returncode
    (output / 'COMMANDS.json').write_text(json.dumps(commands, indent=2) + '\n')
    if status:
        raise SystemExit(status)
    print(f'Ten original events passed, including ownership-negative controls: {output}')


if __name__ == '__main__':
    main()

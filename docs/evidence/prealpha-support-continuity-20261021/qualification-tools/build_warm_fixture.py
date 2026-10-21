#!/usr/bin/env python3
"""Paired private fixture qualification; plans only unless --execute supplied."""
from pathlib import Path
import argparse
import hashlib
import json
import shlex
import subprocess
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--control', type=Path, default=HERE.parent / 'support-control/libjudas_engine.a')
    parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    output, control = args.output.resolve(), args.control.resolve()
    build = ROOT / 'build'
    flags = {}
    for line in (build / 'CMakeFiles/judas_impact_solver_tests.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    source = HERE / 'WarmGuessChecks.cpp'
    obj = output / 'WarmGuessChecks.cpp.o'
    compile_command = ['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'],
                       *flags['CXX_FLAGS'], '-c', str(source), '-o', str(obj)]
    plans = []
    for name, core in [('baseline', build / 'libjudas_engine.a'), ('control', control)]:
        tokens = shlex.split((build / 'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text())
        tokens = [token for token in tokens if not token.startswith('-Wl,--dependency-file=')]
        matches = sum(token.endswith('/tests/ImpactSolverTests.cpp.o') for token in tokens)
        if matches != 1:
            raise RuntimeError('unexpected fixture template object layout')
        tokens = [str(obj) if token.endswith('/tests/ImpactSolverTests.cpp.o') else
                  str(core) if token == 'libjudas_engine.a' else token for token in tokens]
        tokens[tokens.index('-o') + 1] = str(output / name)
        plans.append({'name': name, 'core': str(core), 'core_sha256': sha(core), 'argv': tokens})
    report = {'source_sha256': sha(source), 'compile': compile_command, 'plans': plans, 'results': []}
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
                linked = subprocess.run(plan['argv'], cwd=build, stdout=stream, stderr=subprocess.STDOUT)
            result = {'name': plan['name'], 'link_exit': linked.returncode}
            if linked.returncode == 0:
                binary = output / plan['name']
                started = time.monotonic()
                with (output / (plan['name'] + '.run.log')).open('w') as stream:
                    checked = subprocess.run([str(binary)], cwd=ROOT, stdout=stream,
                                             stderr=subprocess.STDOUT, timeout=120)
                result.update(exit=checked.returncode, seconds=time.monotonic() - started,
                              binary_sha256=sha(binary))
            report['results'].append(result)
            print(result, flush=True)
    report['source_unchanged'] = sha(source) == report['source_sha256']
    report['cores_unchanged'] = all(sha(Path(plan['core'])) == plan['core_sha256'] for plan in plans)
    (output / 'RESULTS.json').write_text(json.dumps(report, indent=2) + '\n')
    raise SystemExit(0 if report.get('compile_exit') == 0 and len(report['results']) == 2 and
                     all(row.get('exit') == 0 for row in report['results']) and
                     report['source_unchanged'] and report['cores_unchanged'] else 1)


if __name__ == '__main__':
    main()

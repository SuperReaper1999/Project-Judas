#!/usr/bin/env python3
"""Prepare/build fresh private application test drivers; NEVER run the tests.

Uses generated CMake compiler/link recipes exactly, apart from private output
paths and an explicitly selected engine archive. Plans only without --execute.
No normal build objects, sources, engine archives or binaries are overwritten.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import time


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
TARGETS = ('judas_character_lab_tests', 'judas_balance_lab_tests',
           'judas_hearthworld_application_tests', 'judas_save_streaming_tests')


def sha(path: Path) -> str:
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def generated_recipe(build: Path, target: str, token: str):
    folder = build / 'CMakeFiles' / (target + '.dir')
    flags, make = folder / 'flags.make', folder / 'build.make'
    variables = dict(line.split(' = ', 1) for line in flags.read_text().splitlines() if ' = ' in line)
    candidates = [line.strip() for line in make.read_text().splitlines()
                  if line.startswith('\t') and re.search(r'\s-o\s+' + re.escape(token) + r'\s', line)]
    if len(candidates) != 1:
        raise RuntimeError(f'{target}: expected one exact source recipe for {token}')
    command = candidates[0]
    for key, value in variables.items():
        command = command.replace('$(' + key + ')', value)
    if '$(' in command:
        raise RuntimeError('Unexpanded Make variable in ' + command)
    argv = shlex.split(command)
    if argv.count('-o') != 1 or argv.count('-c') != 1:
        raise RuntimeError('Unexpected compiler recipe: ' + command)
    source = Path(argv[argv.index('-c') + 1]).resolve()
    return argv, source, flags, make


def dependency_sources(path: Path) -> list[Path]:
    # Generated GCC .d syntax uses escaped spaces and continued lines. Only
    # repository-local source/header dependencies are retained as source inputs;
    # installed toolchain files are represented by build flags and platform libs.
    if not path.is_file():
        return []
    text = path.read_text().replace('\\\n', ' ')
    _, separator, rhs = text.partition(':')
    if not separator:
        raise RuntimeError('Malformed dependency recipe: ' + str(path))
    inputs = []
    for value in shlex.split(rhs):
        dependency = Path(value)
        if not dependency.is_absolute():
            dependency = path.parent / dependency
        dependency = dependency.resolve()
        if dependency.is_relative_to(ROOT / 'src') and dependency.is_file():
            inputs.append(dependency)
    return sorted(set(inputs))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--archive', type=Path, required=True,
                        help='Exact final core archive, e.g. build/libjudas_engine.a')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--targets', choices=TARGETS, nargs='+', default=TARGETS)
    parser.add_argument('--execute', action='store_true', help='Compile/link serially only; never execute tests')
    args = parser.parse_args()
    build, archive, output = args.build.resolve(), args.archive.resolve(), args.output.resolve()
    if output.exists():
        raise SystemExit('Fresh private output required')
    if not output.is_relative_to(ROOT / '.cache'):
        raise SystemExit('Private output must be inside this repository .cache')
    if not archive.is_file():
        raise SystemExit('Explicit final archive missing')
    frozen_path = HERE / 'baseline/frozen/FROZEN.json'
    frozen = {name: value['sha256'] if isinstance(value, dict) else value
              for name, value in json.loads(frozen_path.read_text())['source_inputs'].items()}
    protected = {str(archive): sha(archive)}
    plans = []
    for target in args.targets:
        folder = build / 'CMakeFiles' / (target + '.dir')
        link_path = folder / 'link.txt'
        original = shlex.split(link_path.read_text())
        tokens = [x for x in original if not x.startswith('-Wl,--dependency-file=')]
        fresh, reused, replacements = [], [], {}
        for token in tokens:
            if not token.endswith('.o'):
                continue
            # Require the actual target's recipe, not a guessed generic compile.
            command, source, flags, make = generated_recipe(build, target, token)
            if source.is_relative_to(ROOT / 'tests'):
                private_object = output / 'objects' / target / Path(token).name
                for option, value in (('-o', str(private_object)), ('-MF', str(private_object) + '.d'),
                                      ('-MT', str(private_object))):
                    if option in command:
                        command[command.index(option) + 1] = value
                fresh.append({'source': str(source), 'source_sha256': sha(source), 'argv': command,
                              'output': str(private_object), 'flags_sha256': sha(flags),
                              'make_sha256': sha(make)})
                replacements[token] = str(private_object)
                protected[str(source)] = sha(source)
            else:
                if not source.is_relative_to(ROOT / 'src'):
                    raise RuntimeError('Unexpected non-test object source: ' + str(source))
                source_relative = source.relative_to(ROOT).as_posix()
                current = sha(source)
                if source_relative not in frozen or frozen[source_relative] != current:
                    raise RuntimeError('Reused implementation source differs from frozen source: ' + source_relative)
                object_path = (build / token).resolve()
                dependencies = dependency_sources(Path(str(object_path) + '.d'))
                dep_rows = []
                for dependency in dependencies:
                    relative = dependency.relative_to(ROOT).as_posix()
                    current_dependency = sha(dependency)
                    # A current header mismatching the frozen engine input makes
                    # reusing this app implementation object unjustified.
                    if relative in frozen and frozen[relative] != current_dependency:
                        raise RuntimeError('Reused implementation header changed: ' + relative)
                    dep_rows.append({'source': str(dependency), 'sha256': current_dependency,
                                     'present_in_frozen_source_manifest': relative in frozen})
                    protected[str(dependency)] = current_dependency
                protected[str(object_path)] = sha(object_path)
                protected[str(source)] = current
                reused.append({'object': str(object_path), 'sha256': sha(object_path),
                               'source': str(source), 'source_sha256': current,
                               'source_matches_frozen_manifest': True,
                               'recorded_compile_argv': command,
                               'dependencies': dep_rows,
                               'dependency_list_available': bool(dependencies)})
        if not fresh or any('.dir/tests/' in token and token.endswith('.o') and token not in replacements
                            for token in tokens):
            raise RuntimeError('No fresh driver or stale test TU remains: ' + target)
        if sum(Path(row['source']).name == 'ValidationMain.cpp' for row in fresh) > 1:
            raise RuntimeError('Ambiguous validation wrapper source')
        engines = sum(x == 'libjudas_engine.a' or x.endswith('/libjudas_engine.a') for x in tokens)
        if not engines:
            raise RuntimeError('Target does not link the explicit engine archive: ' + target)
        selected = [replacements.get(token, token) for token in tokens]
        selected = [str(archive) if token == 'libjudas_engine.a' or token.endswith('/libjudas_engine.a')
                    else token for token in selected]
        selected[selected.index('-o') + 1] = str(output / target)
        # Preserve complete ordered static-library closure and record every
        # reused library/module byte identity, including repeated references.
        libraries = []
        for token in selected:
            if token.endswith(('.a', '.so')) or '.so.' in token:
                path = Path(token)
                if not path.is_absolute():
                    path = build / path
                path = path.resolve()
                protected[str(path)] = sha(path)
                libraries.append({'path': str(path), 'sha256': sha(path)})
        protected[str(link_path)] = sha(link_path)
        plans.append({'target': target, 'compile': fresh, 'reused_implementation_objects': reused,
                      'argv': selected, 'binary': str(output / target), 'libraries_in_link_order': libraries,
                      'original_link_sha256': sha(link_path), 'engine_archive_replacements': engines})
    report = {'schema': 'judas.fresh-application-driver-build.v1', 'execute': args.execute,
              'build': str(build), 'archive': str(archive), 'archive_sha256': sha(archive),
              'frozen_source_manifest_sha256': sha(frozen_path), 'plans': plans,
              'protected_inputs_before': protected, 'results': [], 'tests_run': False,
              'notes': ['Every linked tests/*.cpp source, including ValidationMain when present, is freshly compiled.',
                        'No ValidationMain wrapper is invented for targets whose generated recipes do not contain it.',
                        'Reused app implementation objects are hashed; sources/available repository header dependencies match the frozen source manifest.',
                        'Successful compile/link is not behavioural validation. Test execution belongs to the root process.']}
    if not args.execute:
        print(json.dumps(report, indent=2))
        return
    output.mkdir(parents=True)
    def save():
        (output / 'BUILD.json').write_text(json.dumps(report, indent=2) + '\n')
    save()
    for plan in plans:
        status = {'target': plan['target'], 'compiles': []}
        started = time.monotonic()
        for index, entry in enumerate(plan['compile']):
            Path(entry['output']).parent.mkdir(parents=True, exist_ok=True)
            with (output / (plan['target'] + f'.{index}.compile.log')).open('w') as stream:
                compiled = subprocess.run(entry['argv'], cwd=build, stdout=stream, stderr=subprocess.STDOUT)
            status['compiles'].append({'source': entry['source'], 'exit': compiled.returncode})
            if compiled.returncode:
                report['results'].append(status)
                save()
                raise SystemExit(1)
        with (output / (plan['target'] + '.link.log')).open('w') as stream:
            linked = subprocess.run(plan['argv'], cwd=build, stdout=stream, stderr=subprocess.STDOUT)
        status.update(link_exit=linked.returncode, seconds=time.monotonic() - started)
        if linked.returncode == 0:
            status['binary_sha256'] = sha(Path(plan['binary']))
        report['results'].append(status)
        save()
        if linked.returncode:
            raise SystemExit(1)
    report['protected_inputs_unchanged'] = all(Path(path).is_file() and sha(Path(path)) == value
                                             for path, value in protected.items())
    report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    report['build_completed_no_tests_run'] = report['protected_inputs_unchanged'] and len(report['results']) == len(plans)
    save()
    print(json.dumps({'report': str(output / 'BUILD.json'),
                      'build_completed_no_tests_run': report['build_completed_no_tests_run'],
                      'binaries': [plan['binary'] for plan in plans]}, indent=2))
    raise SystemExit(0 if report['build_completed_no_tests_run'] else 1)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Focused M86 receipts and exact source/binary fingerprints.

This tool never builds, downloads, edits Git history or promotes a branch.
Checks run sequentially. Run CPU performance on an otherwise idle Linux host,
not alongside a build, VM benchmark or another validation process.
"""
import argparse
import datetime
import hashlib
import json
import pathlib
import platform
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = '7d1110ee323e3f7d6a7b5d84be338edf144546dc'
FOCUSED = [
    'judas_weather_tests', 'judas_weather_authoring_tests',
    'judas_weather_world_tests', 'judas_weather_streaming_tests',
    'judas_weather_script_tests',
]
REGRESSIONS = [
    'judas_atmosphere_tests', 'judas_atmospheric_flight_tests',
    'judas_character_motor_tests', 'judas_gravity_selection_tests',
    'judas_planet_world_tests', 'judas_planet_streaming_tests',
    'judas_origin_physics_tests', 'judas_rigid_contact_tests',
    'judas_streaming_tests', 'judas_save_storage_tests',
    'judas_audio_world_tests',
    'judas_prefab_tests', 'judas_script_tests',
]
GRAPHICS = ['judas_weather_render_tests', 'judas_weather_application_tests']
GRAPHICS_REGRESSIONS = [
    'judas_render_camera_tests', 'judas_portal_camera_tests',
]
PERFORMANCE = 'judas_weather_performance_tests'
NEW_SOURCES = [
    'src/Weather.h', 'src/Weather.cpp', 'src/WorldWeather.cpp',
    'src/RendererWeather.inl',
    'tests/WeatherTests.cpp', 'tests/WeatherAuthoringTests.cpp',
    'tests/WeatherWorldTests.cpp', 'tests/WeatherStreamingTests.cpp',
    'tests/WeatherScriptTests.cpp', 'tests/WeatherRenderTests.cpp',
    'tests/WeatherApplicationTests.cpp', 'tests/WeatherPerformanceTests.cpp',
    'tools/CreateWeatherLab.py', 'scripts/m86_validation.py',
    'docs/M86_WEATHER.md', 'docs/judasjs/weather.md',
    'docs/judasjs/examples/weather.js',
]
PROJECT = ROOT / 'projects/weather_lab'
PROJECT_EXTENSIONS = {
    '.judasproj', '.judas', '.js', '.judasui', '.judasmeta', '.judasworld',
    '.judasprefab', '.ttf', '.txt', '.md', '.wav', '.png', '.jpg', '.glb',
}
EXCLUDED_PARTS = {
    'saves', '.cache', '__pycache__', '.git', 'build', 'build-release',
    'fixtures', 'generated-fixtures', 'editorprefs', '.judas',
}


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def git(*arguments):
    return subprocess.check_output(['git', *arguments], cwd=ROOT, text=True).strip()


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def excluded(relative):
    path = pathlib.PurePosixPath(relative)
    return (any(part.lower() in EXCLUDED_PARTS for part in path.parts)
            or relative.startswith('docs/evidence/')
            or path.name.lower().endswith(('.judaseditor', '.judasuser', '.user', '.tmp')))


def inventory(build):
    # Tracked differences are authoritative. Untracked historical experiments
    # are never swept in: only this explicit new-file list and this lab are added.
    changed = git('diff', '--name-only', BASE).splitlines()
    candidates = set(changed + NEW_SOURCES)
    for name in NEW_SOURCES:
        if not (ROOT / name).is_file():
            raise RuntimeError('required M86 source is absent: ' + name)
    for path in PROJECT.rglob('*'):
        if path.is_file() and path.suffix.lower() in PROJECT_EXTENSIONS:
            candidates.add(path.relative_to(ROOT).as_posix())
    sources = sorted(name for name in candidates
                     if name and not excluded(name) and (ROOT / name).is_file())
    deleted = sorted(name for name in changed
                     if name and not excluded(name) and not (ROOT / name).exists())
    names = FOCUSED + REGRESSIONS + GRAPHICS + GRAPHICS_REGRESSIONS + [
        PERFORMANCE, 'judas_save_exclusion_tests', 'judasjs_examples_tests', 'judas', 'judas_editor', 'judas_export']
    binaries = {}
    binary_paths = {}
    for name in sorted(set(names)):
        path = build / name
        if not path.is_file() and path.with_suffix('.exe').is_file():
            path = path.with_suffix('.exe')
        if path.is_file():
            binaries[name] = digest(path)
            binary_paths[name] = str(path)
    return {
        'startingCheckpoint': BASE,
        'capturedHEAD': git('rev-parse', 'HEAD'),
        'capturedBranch': git('branch', '--show-current'),
        'sources': {name: digest(ROOT / name) for name in sources},
        'deletedSources': deleted,
        'binaries': binaries,
        'binaryPaths': binary_paths,
        'buildDirectory': str(build),
        'associationNote': ('Hashes identify exact observed files. The separate '
                            'Release build receipt establishes source-to-binary provenance.'),
    }


def unique_path(path):
    # Preserve a previous failing receipt instead of overwriting it on rerun.
    if not path.exists():
        return path
    for index in range(2, 100000):
        candidate = path.with_name(path.stem + '.run-' + str(index) + path.suffix)
        if not candidate.exists():
            return candidate
    raise RuntimeError('receipt path allocation exhausted')


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def verify(expected_path, current, out):
    expected = json.loads(expected_path.read_text())
    differences = []
    if expected.get('startingCheckpoint') != BASE:
        differences.append({'kind': 'startingCheckpoint', 'expected': expected.get('startingCheckpoint'), 'actual': BASE})
    for group in ('sources', 'binaries'):
        old, new = expected.get(group, {}), current[group]
        for name in sorted(set(old) | set(new)):
            if old.get(name) != new.get(name):
                differences.append({'kind': group, 'path': name,
                                    'expected': old.get(name), 'actual': new.get(name)})
    if expected.get('deletedSources', []) != current['deletedSources']:
        differences.append({'kind': 'deletedSources', 'expected': expected.get('deletedSources', []),
                            'actual': current['deletedSources']})
    record = {'verifiedAtUTC': utc(), 'expectedManifest': str(expected_path),
              'currentHEAD': current['capturedHEAD'], 'matches': not differences,
              'differences': differences}
    result = unique_path(out / 'fingerprint-verification.json')
    write_json(result, record)
    print('Fingerprints:', 'MATCH' if not differences else 'MISMATCH', flush=True)
    for item in differences:
        print(json.dumps(item), flush=True)
    return 1 if differences else 0


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--build-dir', default='build')
    parser.add_argument('--output', default='docs/evidence/m86/final')
    parser.add_argument('--regressions', action='store_true')
    parser.add_argument('--graphics', action='store_true')
    parser.add_argument('--typescript', help='Existing standard TypeScript checker module; no installation is performed.')
    parser.add_argument('--performance', action='store_true', help='Run the isolated CPU workload after other headless checks.')
    parser.add_argument('--performance-only', action='store_true', help='Only collect the separate CPU benchmark; do not also run graphics.')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--fingerprints-only', action='store_true')
    mode.add_argument('--verify-fingerprints', nargs='?', const='', metavar='MANIFEST',
                      help='Verify existing fingerprints (default output/fingerprints.json); never replace expected hashes.')
    args = parser.parse_args()
    if args.performance_only and (args.graphics or args.regressions or args.typescript):
        parser.error('--performance-only cannot combine with graphics/regressions/TypeScript')
    out = pathlib.Path(args.output).resolve()
    out.mkdir(parents=True, exist_ok=True)
    build = (ROOT / args.build_dir).resolve()
    initial = inventory(build)
    if args.verify_fingerprints is not None:
        expected = pathlib.Path(args.verify_fingerprints).resolve() if args.verify_fingerprints else out / 'fingerprints.json'
        return verify(expected, initial, out)
    results = []
    cpu_started = cpu_finished = None

    def run(name, arguments):
        log = unique_path(out / (name + '.log'))
        started = utc()
        began = time.monotonic()
        command = [str(value) for value in arguments]
        with log.open('w') as stream:
            stream.write('Started UTC: ' + started + '\nCommand: ' + json.dumps(command) + '\n')
            stream.flush()
            try:
                completed = subprocess.run(command, cwd=ROOT, stdout=stream,
                                           stderr=subprocess.STDOUT, check=False)
                code = completed.returncode
            except OSError as error:
                stream.write(str(error) + '\n')
                code = 127
        finished = utc()
        results.append({'check': name, 'exit': code, 'startedAtUTC': started,
                        'finishedAtUTC': finished, 'durationSeconds': time.monotonic() - began,
                        'command': command, 'log': str(log)})
        print(name, code, flush=True)
        return started, finished

    if not args.fingerprints_only:
        if not args.performance_only:
            for name in FOCUSED + (REGRESSIONS if args.regressions else []):
                run(name, [build / name])
            if args.regressions:
                run('judas_save_exclusion_tests', [build / 'judas_save_exclusion_tests',
                    out / ('save-exclusions-' + str(time.monotonic_ns()))])
            if args.typescript:
                run('api_drift', ['node', ROOT / 'scripts/check_judasjs_api.mjs',
                                  '--typescript', args.typescript])
        if args.performance or args.performance_only:
            print('CPU performance begins now; keep other CPU-intensive workloads stopped.', flush=True)
            cpu_started, cpu_finished = run(PERFORMANCE, [build / PERFORMANCE])
        if args.graphics:
            run('weather_render', [build / GRAPHICS[0], out / 'render-frames'])
            run('weather_application', [build / GRAPHICS[1],
                                       PROJECT / 'weather_lab.judasproj', out / 'application-frames'])
            if args.regressions:
                for name in GRAPHICS_REGRESSIONS:
                    run(name, [build / name])
        report = {
            'startingCheckpoint': BASE, 'capturedHEAD': initial['capturedHEAD'],
            'host': platform.platform(), 'python': platform.python_version(),
            'finishedAtUTC': utc(), 'checks': results,
            'cpuPerformanceStartedAtUTC': cpu_started,
            'cpuPerformanceFinishedAtUTC': cpu_finished,
            'performanceNote': 'Sequential native Linux reference; Windows VM is not a performance acceptance platform.',
        }
        write_json(unique_path(out / 'results.json'), report)
    final = inventory(build)
    if initial['sources'] != final['sources'] or initial['binaries'] != final['binaries']:
        write_json(unique_path(out / 'concurrent-source-change.json'),
                   {'before': initial, 'after': final, 'recordedAtUTC': utc()})
        print('Source/binary files changed during validation; refusing final fingerprint publication.', file=sys.stderr)
        return 2
    final['capturedAtUTC'] = utc()
    destination = out / 'fingerprints.json'
    previous_record = json.loads(destination.read_text()) if destination.exists() else {}
    final['cpuPerformanceStartedAtUTC'] = cpu_started or previous_record.get('cpuPerformanceStartedAtUTC')
    final['cpuPerformanceFinishedAtUTC'] = cpu_finished or previous_record.get('cpuPerformanceFinishedAtUTC')
    if destination.exists():
        previous = unique_path(out / 'fingerprints.previous.json')
        previous.write_bytes(destination.read_bytes())
    write_json(destination, final)
    (out / 'changed-files.txt').write_text('\n'.join(final['sources']) + '\n')
    print('Recorded', len(final['sources']), 'sources and', len(final['binaries']), 'binaries.', flush=True)
    return int(any(result['exit'] for result in results))


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as error:
        print('M86 validation:', error, file=sys.stderr)
        sys.exit(2)

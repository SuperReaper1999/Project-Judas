#!/usr/bin/env python3
"""M69's single incremental Release build and focused pass; stop on first failure.

Reuses the existing build configuration; does not run production suites or retry.
Output must be fresh, so a completed/failed gate cannot be overwritten by accident.
Node/TypeScript are developer tooling only, not game runtime dependencies.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
START = 'c042797c755df68b45e36aa917c4ca72818b9bcd'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / '.cache/m69/gate')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--node', required=True)
    parser.add_argument('--typescript', required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        parser.error('Preserve the existing gate. A second cycle requires operator instruction.')
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if head != START:
        parser.error('Starting checkpoint changed; inspect before building.')
    output.mkdir(parents=True)
    # Freeze affected implementation/tooling/docs before the first build. Evidence
    # reports are intentionally excluded: they will record the gate's actual outcome.
    tracked = subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], cwd=ROOT, text=True).splitlines()
    new = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines()
    paths = sorted(p for p in set(tracked + new) if not p.startswith('docs/evidence/') and (ROOT / p).is_file())
    hashes = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths}
    evidence = ROOT / 'docs/evidence/m69'
    evidence.mkdir(parents=True, exist_ok=True)
    freeze = {'startingHead': START, 'algorithm': 'sha256', 'files': hashes}
    (evidence / 'FINAL_FINGERPRINTS.json').write_text(json.dumps(freeze, indent=2) + '\n')
    patch = subprocess.check_output(['git', 'diff', 'HEAD'], cwd=ROOT, text=True)
    for path in sorted(p for p in new if p in hashes):
        # git diff's exit 1 means the new file differs from /dev/null, not a failure.
        patch += subprocess.run(['git', 'diff', '--no-index', '--', '/dev/null', path],
                                cwd=ROOT, text=True, stdout=subprocess.PIPE).stdout
    (evidence / 'CANDIDATE.diff').write_text(patch)
    summary = {'startingHead': START, 'pass': False, 'commands': [],
               'scope': 'One incremental Release build; paired input/native VM, query batch, two cookbook examples and API/type/live checks. No production suite or hardware-feel claim.'}
    env = {k: v for k, v in os.environ.items() if not k.startswith('JUDAS_')}
    env.update(SDL_VIDEODRIVER='x11', SDL_AUDIODRIVER='dummy',
               XDG_DATA_HOME=str(output / 'user-data'))

    def run(name, command, timeout=240, extra=None):
        command = [str(p) for p in command]
        row = {'name': name, 'command': command, 'cwd': str(ROOT)}
        summary['commands'].append(row)
        print('RUN ' + name, flush=True)
        start = time.monotonic()
        try:
            with (output / (name + '.log')).open('w') as log:
                result = subprocess.run(command, cwd=ROOT, env=dict(env, **(extra or {})), stdout=log,
                                        stderr=subprocess.STDOUT, timeout=timeout)
            row.update(exitCode=result.returncode, seconds=time.monotonic() - start)
            if result.returncode:
                raise RuntimeError(f'{name} failed ({result.returncode}); STOP, no retry. See {output / (name + ".log")}')
        except subprocess.TimeoutExpired:
            row.update(timeout=True, seconds=time.monotonic() - start)
            raise RuntimeError(f'{name} timed out; STOP, no retry.')
        print('PASS ' + name, flush=True)

    try:
        # Generate new target rules before Make parses the multi-target request.
        # Automatic regeneration inside an existing target leaves its running
        # parent Make process with the old rules. Reuse the cached Release setup.
        run('generate-targets', ['cmake', '-S', ROOT, '-B', ROOT / 'build'])
        run('release-build', ['cmake', '--build', ROOT / 'build', '--parallel', args.jobs,
                             '--target', 'judas', 'judas_editor', 'judas_scene_author',
                             'judas_paired_input_tests', 'judas_paired_input_script_tests',
                             'judas_physics_batch_script_tests', 'judasjs_examples_tests'], 1800)
        run('paired-native', [ROOT / 'build/judas_paired_input_tests', output / 'paired-native'])
        run('paired-vm', [ROOT / 'build/judas_paired_input_script_tests', output / 'paired-vm'])
        run('batch-vm', [ROOT / 'build/judas_physics_batch_script_tests', output / 'batch-vm'])
        for example in ('paired-stick', 'ray-fan', 'surface'):
            run('cookbook-' + example, [ROOT / 'build/judasjs_examples_tests', output / 'examples', example])
        run('api-types-live', [args.node, ROOT / 'scripts/check_judasjs_api.mjs',
                              '--typescript', args.typescript, '--runtime', output / 'examples/surface.json'])
        run('create-example', ['python3', ROOT / 'scripts/create_m69_example.py',
                               '--output', ROOT / '.cache/m69/example'])
        steps = output / 'example.steps'
        steps.write_text('REALTIME 90\nLOG_EVERY 0\nACTION compare_rays 1 10 60\n'
                         f'SCREENSHOT 89 {output / "example.png"}\n')
        run('example-startup', [ROOT / 'build/judas', ROOT / '.cache/m69/example/m69_example.judasproj'],
            extra={'JUDAS_TEST_SCRIPT': str(steps)})
        changed = [p for p, h in hashes.items() if not (ROOT / p).is_file() or hashlib.sha256((ROOT / p).read_bytes()).hexdigest() != h]
        if changed:
            raise RuntimeError('Candidate changed during gate: ' + ', '.join(changed))
        summary['pass'] = True
    except Exception as error:
        summary['failure'] = str(error)
        print(str(error), flush=True)
    finally:
        (output / 'RESULTS.json').write_text(json.dumps(summary, indent=2) + '\n')
    return 0 if summary['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""Private control qualification. Plans only unless --execute is explicit.

Reuse existing compiled test objects. Replace DIRECT coordinator/event objects
and engine archives; both current world implementations are mandatory. A stale,
missing or ambiguous coordinator/event path is a hard error.
Original source/build objects/binaries/archives are never modified.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
TARGETS = ['judas_candidate_reuse_tests', 'judas_broadphase_tests',
           'judas_parallel_narrowphase_tests', 'judas_support_guess_ownership_tests',
           'judas_touch_event_tests', 'judas_impact_candidate_cache_tests',
           'judas_growing_impact_tests', 'judas_tracked_contact_rows_tests',
           'judas_impact_solver_tests', 'judas_impact_timing_tests', 'judas_impact_robustness_tests',
           'judas_support_continuation_tests', 'judas_collision_continuity_tests', 'judas_joint_tests',
           'judas_kinematic_tests', 'judas_rigid_contact_tests', 'judas_character_motor_tests',
           'judas_ragdoll_tests', 'judas_physics_continuation_tests', 'judas_contact_lifecycle_tests',
           'judas_contact_cache_tests', 'judas_native_mesh_growth_tests', 'judas_collision_geometry_tests',
           'judas_impact_event_schedule_tests', 'judas_origin_physics_tests', 'judas_origin_assembly_tests',
           'judas_wheel_tests', 'judas_powertrain_tests',
           'judas_structural_tests', 'judas_bending_tests', 'judas_gravity_selection_tests']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', type=Path, default=HERE / 'support-control')
    parser.add_argument('--archive', type=Path, help='Candidate engine archive; overrides --control folder default')
    parser.add_argument('--event-object', type=Path,
                        help='Candidate WorldImpactEvents object for directly compiled world targets')
    parser.add_argument('--world-object', type=Path, required=True,
                        help='Candidate PhysicsWorld object, required to match archive member exactly')
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--targets', nargs='+', default=TARGETS)
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--fresh-drivers', action='store_true',
                        help='Compile every tests/*.cpp TU from its exact CMake recipe into private output')
    args = parser.parse_args()
    control, build, output = args.control.resolve(), args.build.resolve(), args.output.resolve()
    archive = args.archive.resolve() if args.archive else control / 'libjudas_engine.a'
    event_object = args.event_object.resolve() if args.event_object else control / 'WorldImpactEvents.cpp.o'
    world_object = args.world_object.resolve()
    for path in [archive, event_object, world_object]:
        if not path.is_file():
            raise RuntimeError(f'control input missing: {path}')
    # Direct-world substitution must be the SAME object as the archive's world
    # implementation, rather than an old private experiment with a new library.
    members = subprocess.check_output(['/usr/bin/ar', 't', str(archive)], text=True).splitlines()
    if members.count('WorldImpactEvents.cpp.o') != 1:
        raise RuntimeError('candidate archive has missing/ambiguous WorldImpactEvents member')
    archived_event = subprocess.check_output(['/usr/bin/ar', 'p', str(archive), 'WorldImpactEvents.cpp.o'])
    if hashlib.sha256(archived_event).hexdigest() != sha(event_object):
        raise RuntimeError('direct event object does not match candidate archive member')
    if members.count('PhysicsWorld.cpp.o') != 1:
        raise RuntimeError('candidate archive has missing/ambiguous PhysicsWorld member')
    archived_world = subprocess.check_output(['/usr/bin/ar', 'p', str(archive), 'PhysicsWorld.cpp.o'])
    if hashlib.sha256(archived_world).hexdigest() != sha(world_object):
        raise RuntimeError('direct coordinator object does not match candidate archive member')
    plans = []
    protected = {str(path): sha(path) for path in [archive, event_object, world_object, build / 'libjudas_engine.a']}
    for target in args.targets:
        recipe = build / f'CMakeFiles/{target}.dir/link.txt'
        tokens = shlex.split(recipe.read_text())
        tokens = [token for token in tokens if not token.startswith('-Wl,--dependency-file=')]
        direct_event, direct_world, engines = 0, 0, 0
        selected = []
        for token in tokens:
            if token.endswith('/src/PhysicsWorld.cpp.o'):
                direct_world += 1
                original = build / token
                protected[str(original)] = sha(original)
                selected.append(str(world_object))
            elif token.endswith('/src/physics/WorldImpactEvents.cpp.o'):
                direct_event += 1
                original = build / token
                protected[str(original)] = sha(original)
                selected.append(str(event_object))
            elif token == 'libjudas_engine.a' or token.endswith('/libjudas_engine.a'):
                engines += 1
                selected.append(str(archive))
            else:
                selected.append(token)
        # CMake may repeat static libraries to close dependency cycles. Replace
        # every occurrence while preserving order, rather than rejecting this.
        if (direct_event > 1 or direct_world > 1 or
                not (direct_event or engines) or not (direct_world or engines)):
            raise RuntimeError(f'{target}: ambiguous/absent production-world path: '
                               f'direct_coordinator={direct_world}, direct_event={direct_event}, '
                               f'archive={engines}')
        selected[selected.index('-o') + 1] = str(output / target)
        compiles = []
        if args.fresh_drivers:
            flagfile = build / f'CMakeFiles/{target}.dir/flags.make'
            variables = dict(line.split(' = ', 1) for line in flagfile.read_text().splitlines() if ' = ' in line)
            make_lines = (build / f'CMakeFiles/{target}.dir/build.make').read_text().splitlines()
            replacements = {}
            for token in selected:
                if '.dir/tests/' not in token or not token.endswith('.cpp.o'):
                    continue
                # Use the actual generated source recipe, not an invented
                # compilation that loses -Dmain/wrapper or per-source options.
                candidates = [line.strip() for line in make_lines if line.startswith('\t') and
                              re.search(r'\s-o\s+' + re.escape(token) + r'\s', line)]
                if len(candidates) != 1:
                    raise RuntimeError(f'{target}: no unique compile recipe for {token}')
                command = candidates[0]
                for key, value in variables.items():
                    command = command.replace('$(' + key + ')', value)
                if '$(' in command:
                    raise RuntimeError('unexpanded Make variable in driver recipe: ' + command)
                command = shlex.split(command)
                private = output / 'objects' / target / Path(token).name
                for option, value in [('-o', str(private)), ('-MF', str(private) + '.d'), ('-MT', str(private))]:
                    if option in command:
                        command[command.index(option) + 1] = value
                source_path = Path(command[command.index('-c') + 1])
                compiles.append({'argv': command, 'source': str(source_path), 'source_sha256': sha(source_path),
                                 'flags_sha256': sha(flagfile), 'output': str(private)})
                replacements[token] = str(private)
            selected = [replacements.get(token, token) for token in selected]
            if not compiles:
                raise RuntimeError(f'{target}: no fresh test driver selected')
        plans.append({'target': target, 'argv': selected, 'cwd': str(build),
                      'driver_compiles': compiles,
                      'direct_world_event_replacements': direct_event,
                      'direct_world_coordinator_replacements': direct_world,
                      'engine_archive_replacements': engines,
                      'original_link_sha256': sha(recipe)})
    report = {'schema': 'judas.private-current-world-focused.v2',
              'candidate_archive': str(archive), 'candidate_event_object': str(event_object),
              'candidate_world_object': str(world_object),
              'direct_event_object_matches_archive': True,
              'direct_world_object_matches_archive': True,
              'execute': args.execute, 'control_archive_sha256': sha(archive),
              'control_event_object_sha256': sha(event_object),
              'control_world_object_sha256': sha(world_object), 'plans': plans,
              'notes': ['Existing test objects are reused; every target must consume the actual control world implementation.',
                        'Coordinator and event objects must byte-match their unique archive members; no stale directly compiled world path is accepted.',
                        'Run only after root grants the sole CPU workload slot.'], 'results': []}
    if not args.execute:
        print(json.dumps(report, indent=2))
        return
    if output.exists():
        raise RuntimeError('fresh output required')
    output.mkdir(parents=True)
    env = os.environ.copy()
    env['SDL_AUDIODRIVER'] = 'dummy'
    for plan in plans:
        started = time.monotonic()
        compile_status = []
        for entry in plan['driver_compiles']:
            Path(entry['output']).parent.mkdir(parents=True, exist_ok=True)
            log = output / (plan['target'] + '.' + Path(entry['output']).name + '.compile.log')
            with log.open('w') as stream:
                compiled = subprocess.run(entry['argv'], cwd=build, stdout=stream, stderr=subprocess.STDOUT)
            compile_status.append({'source': entry['source'], 'exit': compiled.returncode})
            if compiled.returncode:
                report['results'].append({'target': plan['target'], 'driver_compiles': compile_status,
                                          'compile_failure': True, 'link_exit': None})
                (output / 'RESULTS.json').write_text(json.dumps(report, indent=2) + '\n')
                raise SystemExit(1)
        log = output / (plan['target'] + '.link.log')
        with log.open('w') as stream:
            linked = subprocess.run(plan['argv'], cwd=build, stdout=stream, stderr=subprocess.STDOUT, check=False)
        result = {'target': plan['target'], 'link_exit': linked.returncode,
                  'link_seconds': time.monotonic() - started, 'driver_compiles': compile_status}
        if linked.returncode == 0:
            binary = output / plan['target']
            started = time.monotonic()
            with (output / (plan['target'] + '.run.log')).open('w') as stream:
                try:
                    checked = subprocess.run([str(binary)], cwd=ROOT, env=env, stdout=stream,
                                             stderr=subprocess.STDOUT, timeout=240, check=False)
                    result['exit'] = checked.returncode
                except subprocess.TimeoutExpired:
                    result['exit'] = None
                    result['timed_out'] = True
            result.update(binary_sha256=sha(binary), seconds=time.monotonic() - started)
        report['results'].append(result)
        report['utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        (output / 'RESULTS.json').write_text(json.dumps(report, indent=2) + '\n')
        print(result, flush=True)
        if linked.returncode or result.get('exit') != 0:
            break
    report['protected_inputs_unchanged'] = all(sha(Path(path)) == value for path, value in protected.items())
    report['pass_all'] = len(report['results']) == len(plans) and all(
        row['link_exit'] == 0 and row.get('exit') == 0 for row in report['results']) and report['protected_inputs_unchanged']
    (output / 'RESULTS.json').write_text(json.dumps(report, indent=2) + '\n')
    raise SystemExit(0 if report['pass_all'] else 1)


if __name__ == '__main__':
    main()

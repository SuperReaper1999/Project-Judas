#!/usr/bin/env python3
"""Owned exact-native computational closure diagnostic; not production code."""
import pathlib, shlex, subprocess, hashlib, shutil, os, time, json

r = pathlib.Path.cwd()
base = r / '.cache/m78-followup/release'
owned = r / '.cache/m78-cold-boundary/first-boundary-closure'
ev = r / 'docs/evidence/m78-cold-boundary'
receipts = []
h = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

def flags(target):
    result = {}
    for line in (base / ('CMakeFiles/' + target + '.dir/flags.make')).read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            result[key] = shlex.split(value)
    return result

def run(command, log, **kwargs):
    start = time.perf_counter()
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, **kwargs)
    (ev / log).write_text(result.stdout)
    receipts.append(dict(command=command, exitCode=result.returncode,
                         seconds=time.perf_counter() - start))
    return result

for source, target in [('JointSolver.cpp', 'judas_engine'),
                       ('PhysicalBalance.cpp', 'judas_engine'),
                       ('BalanceLabBoundaryObserver.cpp', 'judas_balance_lab_tests')]:
    f = flags(target)
    command = ['c++', *f['CXX_DEFINES'], *f['CXX_INCLUDES'],
               '-I' + str(r / 'third_party'), *f['CXX_FLAGS'],
               '-c', str(owned / source), '-o', str(owned / (source + '.o'))]
    result = run(command, 'first-closure-' + source + '-compile.log')
    assert result.returncode == 0, result.stdout

for name, normal_prepare in [('commands-seeds', False),
                              ('commands-seeds-normal-prepare', True)]:
    variant = owned / name
    variant.mkdir(exist_ok=False)
    archive = variant / 'libjudas_engine.a'
    shutil.copyfile(base / 'libjudas_engine.a', archive)
    objects = [str(owned / 'JointSolver.cpp.o')]
    if normal_prepare:
        objects.append(str(owned / 'PhysicalBalance.cpp.o'))
    subprocess.run(['ar', 'r', str(archive), *objects], check=True)
    command = shlex.split((base / 'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text())
    command = [x for x in command if not x.startswith('-Wl,--dependency-file=')]
    command[command.index('CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o')] = str(owned / 'BalanceLabBoundaryObserver.cpp.o')
    command[command.index('libjudas_engine.a')] = str(archive)
    command[command.index('-o') + 1] = str(variant / 'observer')
    result = run(command, 'first-closure-' + name + '-link.log', cwd=base)
    assert result.returncode == 0, result.stdout

    out = variant / 'native-restored'
    (out / 'slots').mkdir(parents=True)
    shutil.copytree(r / 'projects/balance_lab', out / 'project')
    snapshot = r / 'docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save'
    shutil.copyfile(snapshot, out / 'slots/m77.save')
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy',
               LIBGL_ALWAYS_SOFTWARE='1', JUDAS_BOUNDARY_REPLAY_CAPTURED_NATIVE='1',
               JUDAS_BOUNDARY_STEPS='1', XDG_DATA_HOME=str(variant / 'data'),
               XDG_CONFIG_HOME=str(variant / 'config'))
    command = [str(variant / 'observer'), str(out), 'cold-read']
    result = run(command, 'first-closure-' + name + '-run.log', env=env)
    receipts[-1].update(snapshotSHA256=h(out / 'slots/m77.save'),
                        archiveSHA256=h(archive), exeSHA256=h(variant / 'observer'),
                        normalPrepareDiagnostic=normal_prepare)
    print(name, result.returncode, flush=True)
    print('\n'.join(x for x in result.stdout.splitlines()
                    if 'STATE_OBSERVER' in x or 'ACTOR 12' in x or 'FAIL' in x), flush=True)
    for source in ['joint-boundary.jsonl', 'physical-immediately-restored.json',
                   'physical-final.json', 'diagnostic-cold-read-trace.jsonl']:
        if (out / source).exists():
            shutil.copyfile(out / source, ev / ('linux-first-closure-' + name + '-' + source))
    for source in out.glob('physical-step-*.json'):
        shutil.copyfile(source, ev / ('linux-first-closure-' + name + '-' + source.name))
    if result.returncode not in (0, 1):
        raise RuntimeError('Closure guard/runtime failed: ' + result.stdout[-2000:])

identity = json.loads((owned / 'IDENTITY.json').read_text())
identity['steps'] = receipts
(ev / 'linux-first-closure-isolation.json').write_text(json.dumps(identity, indent=2) + '\n')

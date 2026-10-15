#!/usr/bin/env python3
"""Compile an unchanged preserved observer against current final archives.

Default is preparation only. --run is reserved for the root's serialized Linux
validation window. No tracked source or historical evidence is modified.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
ORIGINAL = ROOT / 'docs/evidence/m78-cold-boundary/final-current-standing/FinalCurrentStandingObserver.cpp'
EXPECTED = '715dd4ae56bc5a8b9556379d95bb5b6e8f4402fd4a60515b69ba039f0b918e13'


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--run', action='store_true')
    parser.add_argument('--output', default='/tmp/judas-m87-final/cold')
    args = parser.parse_args()
    output = Path(args.output).absolute()
    output.mkdir(parents=True, exist_ok=True)
    if sha(ORIGINAL) != EXPECTED:
        raise RuntimeError('Preserved observer source differs from its recorded historical identity')
    source = output / 'FinalCurrentStandingObserver.cpp'
    shutil.copyfile(ORIGINAL, source)
    if sha(source) != EXPECTED:
        raise RuntimeError('Copied observer bytes do not match preserved source')
    flags_file = BUILD / 'CMakeFiles/judas_balance_lab_tests.dir/flags.make'
    link_file = BUILD / 'CMakeFiles/judas_balance_lab_tests.dir/link.txt'
    flags = {}
    for line in flags_file.read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    obj = output / 'observer.o'
    binary = output / 'current-standing-observer'
    compile_command = ['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'],
                       '-I' + str(ROOT / 'third_party'), *flags['CXX_FLAGS'],
                       '-c', str(source), '-o', str(obj)]
    link_command = shlex.split(link_file.read_text())
    link_command = [token for token in link_command if not token.startswith('-Wl,--dependency-file=')]
    original_object = 'CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o'
    if link_command.count(original_object) != 1:
        raise RuntimeError('Cannot identify the single original fixture object in current link command')
    link_command[link_command.index(original_object)] = str(obj)
    link_command[link_command.index('-o') + 1] = str(binary)
    archive_files = [BUILD / token for token in link_command if token.endswith('.a')]
    plan = {'scope': 'Unchanged M70 cold-save checks and exact preserved M77 physical-endpoint observer against current final Linux engine archive',
            'observerOriginal': str(ORIGINAL), 'observerCopy': str(source),
            'observerSHA256': EXPECTED, 'originalTestsUnmodified': True,
            'compile': compile_command, 'link': link_command,
            'compileWorkingDirectory': str(BUILD), 'runWorkingDirectory': str(ROOT),
            'environment': {'JUDAS_DIAGNOSTIC_CONTINUE_WRITE': '1', 'SDL_AUDIODRIVER': 'dummy',
                            'clearedPrefixes': ['JUDAS_DIAGNOSTIC_', 'JUDAS_BOUNDARY_', 'M78_DIAGNOSTIC_']},
            'acceptance': ['Original M70 cold-write/read exit zero in separate processes',
                           'Original M77 cold-write/read exit zero in separate processes',
                           'Saved snapshot exact bytes unchanged by read',
                           'M77 uninterrupted and restored currentPhysicalEndpoint exactly equal',
                           'Both actual physical endpoints derive currentPhysicalStanding=true',
                           'Current progress is Succeeded, both feet have accepted physical contact and original height/speed/upright thresholds pass']}
    (output / 'PLAN.json').write_text(json.dumps(plan, indent=2) + '\n')
    if not args.run:
        print(json.dumps({'prepared': str(output / 'PLAN.json'), 'observerSHA256': EXPECTED,
                          'compiled': False, 'ranTests': False}))
        return

    for fixture in ['character', 'balance']:
        if (output / fixture / 'project').exists():
            raise RuntimeError('Use a fresh owned run output; never overwrite an existing fixture/save')
    receipt = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
               'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'observerSHA256': EXPECTED,
               'characterSourceSHA256': sha(ROOT / 'tests/CharacterLabTests.cpp'),
               'balanceSourceSHA256': sha(ROOT / 'tests/BalanceLabTests.cpp'),
               'sourceFingerprints': {}, 'binaryFingerprints': {}, 'steps': [], 'pass': False}
    for path in [ROOT / 'src/PhysicsWorld.cpp', ROOT / 'src/PhysicsWorld.h', ROOT / 'src/ContactSolver.cpp',
                 ROOT / 'src/JointSolver.cpp', ROOT / 'src/CollisionGeometry.cpp', ROOT / 'src/WorldPersistence.cpp']:
        receipt['sourceFingerprints'][str(path)] = sha(path)
    for path in [*archive_files, BUILD / 'judas_character_lab_tests']:
        receipt['binaryFingerprints'][str(path)] = sha(path)
    env = os.environ.copy()
    for key in list(env):
        if key.startswith(('JUDAS_DIAGNOSTIC_', 'JUDAS_BOUNDARY_', 'M78_DIAGNOSTIC_')):
            del env[key]
    env.update(JUDAS_DIAGNOSTIC_CONTINUE_WRITE='1', SDL_AUDIODRIVER='dummy')

    def run(name, command, cwd):
        before = time.monotonic()
        with (output / (name + '.log')).open('w') as log:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT,
                                    check=False, timeout=600)
        receipt['steps'].append({'name': name, 'command': command, 'workingDirectory': str(cwd),
                                 'exitCode': result.returncode, 'seconds': time.monotonic() - before})
        (output / 'RECEIPT.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(name, 'exit', result.returncode, flush=True)
        if result.returncode:
            raise RuntimeError(name + ' failed; original log/save retained')

    run('observer-compile', compile_command, BUILD)
    run('observer-link', link_command, BUILD)
    receipt['binaryFingerprints'][str(binary)] = sha(binary)
    for name, executable, slot in [('character', BUILD / 'judas_character_lab_tests', 'm70'),
                                   ('balance', binary, 'm77')]:
        fixture = output / name
        run(name + '-cold-write', [str(executable), str(fixture), 'cold-write'], ROOT)
        save = fixture / 'slots' / (slot + '.save')
        if not save.is_file():
            raise RuntimeError('Expected ordinary snapshot file absent: ' + str(save))
        before_save = sha(save)
        shutil.copyfile(save, output / (name + '-snapshot-preserved.save'))
        if name == 'balance':
            shutil.copyfile(fixture / 'physical-final.json', output / 'write-physical-final.json')
        run(name + '-cold-read', [str(executable), str(fixture), 'cold-read'], ROOT)
        after_save = sha(save)
        receipt[name + 'SaveSHA256'] = {'beforeRead': before_save, 'afterRead': after_save,
                                     'unchanged': before_save == after_save}
        if before_save != after_save:
            raise RuntimeError('Cold reader modified original captured save bytes')
        if name == 'balance':
            shutil.copyfile(fixture / 'physical-final.json', output / 'read-physical-final.json')

    write = json.loads((output / 'write-physical-final.json').read_text())['currentPhysicalEndpoint']
    read = json.loads((output / 'read-physical-final.json').read_text())['currentPhysicalEndpoint']
    comparison = {'scope': 'Within this Linux write/read pair, actual completed physical endpoint; no cached controller-status-only verdict',
                  'equal': write == read, 'writeCurrentPhysicalStanding': write['currentPhysicalStanding'],
                  'readCurrentPhysicalStanding': read['currentPhysicalStanding'], 'write': write, 'read': read}
    (output / 'CURRENT_PHYSICAL_ENDPOINT_COMPARISON.json').write_text(json.dumps(comparison, indent=2) + '\n')
    for path, expected in {**receipt['sourceFingerprints'], **receipt['binaryFingerprints']}.items():
        if sha(path) != expected:
            raise RuntimeError('Source/binary changed during serialized validation: ' + path)
    if not comparison['equal'] or not comparison['writeCurrentPhysicalStanding'] or not comparison['readCurrentPhysicalStanding']:
        raise RuntimeError('Actual current physical endpoint/continuation regression; original evidence preserved')
    receipt['pass'] = True
    receipt['endpointComparison'] = {key: comparison[key] for key in ['equal', 'writeCurrentPhysicalStanding', 'readCurrentPhysicalStanding']}
    (output / 'RECEIPT.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'pass': True, 'receipt': str(output / 'RECEIPT.json'), 'endpoint': receipt['endpointComparison']}))


if __name__ == '__main__':
    main()

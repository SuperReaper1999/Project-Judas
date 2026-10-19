#!/usr/bin/env python3
"""Same-source diagnostic against immutable baseline and rejected A2 archives."""
from pathlib import Path
import datetime
import hashlib
import json
import shlex
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
CACHE = ROOT / '.cache/dense-scalability-20261019'
PROTO = CACHE / 'coherent-geometry-prototype'
OUT = CACHE / 'coherent-geometry-qualification/public-history-A2'
OUT.mkdir(exist_ok=False)
SOURCE = OUT / 'PublicMotionReconstructionTests.cpp'
shutil.copy2(PROTO / SOURCE.name, SOURCE)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


flags = {}
for line in (ROOT / 'build/CMakeFiles/judas_rigid_motion_tests.dir/flags.make').read_text().splitlines():
    if line.startswith('CXX_') and ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link_recipe = shlex.split((ROOT / 'build/CMakeFiles/judas_rigid_motion_tests.dir/link.txt').read_text())
link_recipe = [x for x in link_recipe if not x.startswith('-Wl,--dependency-file=')]
original_object = 'CMakeFiles/judas_rigid_motion_tests.dir/tests/RigidMotionTests.cpp.o'
assert link_recipe.count(original_object) == 1
receipt = dict(scope='Same-source public motion history reconstruction diagnostic; no whole-engine qualification',
               utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
               source_sha256=sha(SOURCE), source=str(SOURCE), commands=[], variants={},
               threshold='--require-exact: bit-identical published pose reconstruction; no new tolerance',
               all_native_tests_pass=False)


def run(name, command, cwd):
    begin = time.monotonic()
    log = OUT / (name + '.log')
    with log.open('w') as stream:
        result = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT, timeout=120)
    receipt['commands'].append(dict(name=name, argv=command, cwd=str(cwd), actual_exit=result.returncode,
                                   log=str(log), seconds=time.monotonic() - begin))
    (OUT / 'RESULTS.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return result.returncode


variants = [('baseline', ROOT / 'src', CACHE / 'candidate-fastpaths-final/libjudas_engine.a'),
            ('candidate', PROTO / 'src', CACHE / 'coherent-geometry-candidate-A2/libjudas_engine.a')]
for name, headers, archive in variants:
    obj = OUT / (name + '.o')
    binary = OUT / (name + '-public-history')
    compile_command = ['/usr/bin/c++', *flags['CXX_DEFINES'], '-I' + str(headers),
                       *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'], '-MMD', '-MF', str(obj) + '.d',
                       '-c', str(SOURCE), '-o', str(obj)]
    if run(name + '-compile', compile_command, ROOT) != 0:
        raise SystemExit(2)
    link = [str(obj) if x == original_object else str(archive) if x == 'libjudas_engine.a' else x
            for x in link_recipe]
    link[link.index('-o') + 1] = str(binary)
    if run(name + '-link', link, ROOT / 'build') != 0:
        raise SystemExit(2)
    code = run(name + '-require-exact', [str(binary), '--require-exact'], ROOT)
    receipt['variants'][name] = dict(headers=str(headers), archive=str(archive),
                                    archive_sha256=sha(archive), binary_sha256=sha(binary),
                                    exit=code, native_test_pass=code == 0,
                                    output=(OUT / (name + '-require-exact.log')).read_text())
    print(name, 'exit', code, receipt['variants'][name]['output'], flush=True)
receipt['all_native_tests_pass'] = all(v['native_test_pass'] for v in receipt['variants'].values())
receipt['same_source_bytes_after_execution'] = sha(SOURCE) == receipt['source_sha256']
(OUT / 'RESULTS.json').write_text(json.dumps(receipt, indent=2) + '\n')
raise SystemExit(0 if receipt['all_native_tests_pass'] else 1)

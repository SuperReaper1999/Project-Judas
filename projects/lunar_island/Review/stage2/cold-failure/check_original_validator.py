#!/usr/bin/env python3
"""Negative control: link today's regression against the original validator only.
Writes disposable compilation inputs/outputs under /tmp; no checkout mutation.
"""
from pathlib import Path
import shutil
import shlex
import subprocess

repo = Path(__file__).resolve().parents[5]
base = '5424d85dbcdc8f8a603ea115d7d52d9048520b12'
work = Path('/tmp/lunar-original-validator')
work.mkdir(exist_ok=True)
for header in (repo / 'src/physics').glob('*.h'):
    shutil.copy2(header, work / header.name)
for name in ['WorldInternal.h', 'WorldPersistence.cpp']:
    (work / name).write_bytes(subprocess.check_output(
        ['git', 'show', f'{base}:src/physics/{name}'], cwd=repo))
target = repo / 'build/CMakeFiles/judas_physics_continuation_tests.dir'
flags = {}
for line in (target / 'flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
subprocess.run(['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'],
                *flags['CXX_FLAGS'], '-I' + str(repo / 'src/physics'),
                '-c', str(work / 'WorldPersistence.cpp'), '-o', str(work / 'original.o')], check=True)
originalSnapshot = subprocess.check_output(['git','show',f'{base}:src/WorldPersistence.cpp'],cwd=repo)
(work/'OriginalWorldSnapshot.cpp').write_bytes(originalSnapshot)
source=(repo/'tests/PhysicsContinuationTests.cpp').read_text()
source=source[:source.index('void MassFieldValidation()')]+source[source.index('void PhysicalEntityPersistence(){'):]
source=source.replace('MassFieldValidation();DurableMassProperties();','')
(work/'SignedZeroRegression.cpp').write_text(source)
for name in ['OriginalWorldSnapshot','SignedZeroRegression']:
    subprocess.run(['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(work/(name+'.cpp')),'-o',str(work/(name+'.o'))],check=True)
args = shlex.split((target / 'link.txt').read_text())
args = [arg for arg in args if not arg.startswith('-Wl,--dependency-file=')]
args.insert(1,str(work/'OriginalWorldSnapshot.o'))
args.insert(1, str(work / 'original.o'))
args=[str(work/'SignedZeroRegression.o') if arg.endswith('/tests/PhysicsContinuationTests.cpp.o') else arg for arg in args]
args[args.index('-o') + 1] = str(work / 'regression')
subprocess.run(args, cwd=repo / 'build', check=True)
result = subprocess.run([str(work / 'regression')])
if result.returncode != 1:
    raise SystemExit('Original validator must fail the signed-zero positive control')
print('PASS negative control: unchanged original validator is detected by current regression')

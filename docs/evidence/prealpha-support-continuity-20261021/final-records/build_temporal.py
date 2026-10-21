from pathlib import Path
import datetime
import hashlib
import json
import shlex
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
work = Path(__file__).resolve().parent / 'temporal'
source = root / '.cache/contact-redesign-20261021/temporal'
work.mkdir(exist_ok=True)
files = ['PhysicsStressLiveBenchmark.observer.cpp', 'TemporalObserver.h', 'FloorOracle.h']
for name in files:
    shutil.copy2(source / name, work / name)
build = root / 'build'
flags = {}
for line in (build / 'CMakeFiles/judas_physics_stress_live_benchmark.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
commands = []
def run(command, cwd=root):
    command = list(map(str, command))
    commands.append(command)
    subprocess.run(command, cwd=cwd, check=True)
run(['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'],
     '-I'+str(work), '-c', work / files[0], '-o', work / 'observer.o'])
for name, core in [('baseline', root / '.cache/persistent-contact-20261021/measure/baseline/frozen/libjudas_engine.a'),
                   ('control', root / '.cache/persistent-contact-20261021/support-control/libjudas_engine.a')]:
    link = shlex.split((build / 'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
    link = [x for x in link if not x.startswith('-Wl,--dependency-file=')]
    link[link.index('-o')+1] = str(work / ('observer-'+name))
    link = [str(core) if x=='libjudas_engine.a' else str(work / 'observer.o')
            if x.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o') else x for x in link]
    run(link, build)
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
(work/'BUILD.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'commands':commands, 'files':{name:sha(work/name) for name in files+['observer.o','observer-baseline','observer-control']},
    'observer_sources_byte_identical_to_prior': all(sha(work/name)==sha(source/name) for name in files)}, indent=2)+'\n')

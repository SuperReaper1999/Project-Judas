#!/usr/bin/env python3
"""Original untouched BalanceLab fixture with copied WP passive12 prototype."""
import pathlib, shlex, subprocess, hashlib, shutil, os, time, json

r = pathlib.Path.cwd()
base = r / '.cache/m78-followup/release'
owned = r / '.cache/m78-cold-boundary/new-capture-passive-history'
ev = r / 'docs/evidence/m78-cold-boundary'
receipts = []
h = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
flags = {}
for line in (base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)

def run(command, name, **kwargs):
    start = time.perf_counter()
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
    (ev/('new-capture-passive-' + name + '.log')).write_text(result.stdout)
    receipts.append(dict(command=command, exitCode=result.returncode, seconds=time.perf_counter()-start))
    return result

command = ['c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'],
           '-c', str(owned/'WorldPersistence.cpp'), '-o', str(owned/'WorldPersistence.cpp.o')]
result = run(command, 'compile')
assert result.returncode == 0, result.stdout
archive = owned/'libjudas_engine.a'
shutil.copyfile(base/'libjudas_engine.a', archive)
subprocess.run(['ar','r',str(archive),str(owned/'WorldPersistence.cpp.o')],check=True)
command = shlex.split((base/'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text())
command = [x for x in command if not x.startswith('-Wl,--dependency-file=')]
command[command.index('libjudas_engine.a')] = str(archive)
command[command.index('-o')+1] = str(owned/'original-fixture')
result = run(command, 'link', cwd=base)
assert result.returncode == 0, result.stdout

env = os.environ.copy()
env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',
           XDG_DATA_HOME=str(owned/'data'),XDG_CONFIG_HOME=str(owned/'config'))
for key in list(env):
    if key.startswith('JUDAS_BOUNDARY_') or key.startswith('JUDAS_DIAGNOSTIC_'):
        del env[key]
for mode in ['cold-write', 'cold-read']:
    out = owned/'new-capture'
    result = run([str(owned/'original-fixture'),str(out),mode], mode, env=env)
    receipts[-1]['saveSHA256'] = h(out/'slots/m77.save')
    print(mode,result.returncode,flush=True)
    print('\n'.join(x for x in result.stdout.splitlines() if 'ACTOR 12' in x or 'SUMMARY' in x),flush=True)
    shutil.copyfile(out/'cold-write-state.json', ev/('linux-new-capture-passive-'+mode+'-result.json'))

old = owned/'legacy-a106'
(old/'slots').mkdir(parents=True)
shutil.copytree(r/'projects/balance_lab',old/'project')
shutil.copyfile(r/'docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save',old/'slots/m77.save')
result = run([str(owned/'original-fixture'),str(old),'cold-read'],'legacy-a106-read',env=env)
receipts[-1]['saveSHA256']=h(old/'slots/m77.save')
print('legacy-a106',result.returncode,flush=True)
print('\n'.join(x for x in result.stdout.splitlines() if 'ACTOR 12' in x or 'SUMMARY' in x),flush=True)
identity=json.loads((owned/'IDENTITY.json').read_text())
identity.update(steps=receipts,archiveSHA256=h(archive),exeSHA256=h(owned/'original-fixture'))
(ev/'linux-new-capture-passive-isolation.json').write_text(json.dumps(identity,indent=2)+'\n')

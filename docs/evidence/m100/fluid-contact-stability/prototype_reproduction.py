from pathlib import Path
import importlib.util,subprocess,shlex
r=Path.cwd();o=r/'.cache/m100';source=o/'alpha-source';build=o/'alpha-linux'
spec=importlib.util.spec_from_file_location('baseline',source/'tools/build_linux_baseline.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
prefix=m.environment_command(source,build,rootfs=r/'.cache/prealpha-elastic-linux/ubuntu24');prefix[-1]='/build';prefix+=['--ro-bind',str(r),'/tmp/legacy']
commands=subprocess.check_output(prefix+['ninja','-C','/build','-t','commands','judas_script_tests'],text=True).splitlines()
base=shlex.split(next(x for x in commands if ' -c /source/tests/ScriptTests.cpp' in x))
for name,src in [('local','/tmp/legacy/.cache/m100/fluid-bounded/LocalFootprint.cpp'),('hydro','/tmp/legacy/tests/FluidHydrostaticsTests.cpp')]:
 c=list(base);c[c.index('-c')+1]=src;c[c.index('-o')+1]='/build/legacy-startup/'+name+'.o';c[c.index('-MF')+1]='/build/legacy-startup/'+name+'.d';subprocess.run(prefix+c+['-I/source/src'],check=True)
l=shlex.split(commands[-1]);a=l.index('&&')+1;b=l.index('&&',a);l=l[a:b];l=[('/build/legacy-startup/hydro.o' if x.endswith('/tests/ScriptTests.cpp.o') else x) for x in l];l.insert(l.index('/build/legacy-startup/hydro.o')+1,'/build/legacy-startup/local.o');l[l.index('-o')+1]='/build/legacy-startup/local-hydro';subprocess.run(prefix+l,check=True)
with (o/'fluid-bounded/local-footprint.log').open('w') as f: result=subprocess.run(prefix+['/build/legacy-startup/local-hydro'],stdout=f,stderr=subprocess.STDOUT)
print('prototype exit',result.returncode)
l.remove('/build/legacy-startup/local.o');l[l.index('-o')+1]='/build/legacy-startup/baseline-hydro';subprocess.run(prefix+l,check=True)
with (o/'fluid-bounded/baseline-hydro.log').open('w') as f: result=subprocess.run(prefix+['/build/legacy-startup/baseline-hydro'],stdout=f,stderr=subprocess.STDOUT)
print('baseline exit',result.returncode)

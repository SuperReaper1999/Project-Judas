from pathlib import Path
import importlib.util,subprocess,shlex
r=Path.cwd();o=r/'.cache/m100';source=o/'alpha-source';build=o/'alpha-linux'
spec=importlib.util.spec_from_file_location('baseline',source/'tools/build_linux_baseline.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
prefix=m.environment_command(source,build,rootfs=r/'.cache/prealpha-elastic-linux/ubuntu24');prefix[-1]='/build';prefix+=['--ro-bind',str(r),'/tmp/legacy']
commands=subprocess.check_output(prefix+['ninja','-C','/build','-t','commands','judas_script_tests'],text=True).splitlines()
c=shlex.split(next(x for x in commands if ' -c /source/tests/ScriptTests.cpp' in x));c[c.index('-c')+1]='/build/legacy-startup/full.cpp';c[c.index('-o')+1]='/build/legacy-startup/full.o';c[c.index('-MF')+1]='/build/legacy-startup/full.d'
subprocess.run(prefix+c,check=True)
l=shlex.split(commands[-1]);a=l.index('&&')+1;b=l.index('&&',a);l=l[a:b];l=[('/build/legacy-startup/full.o' if x.endswith('/tests/ScriptTests.cpp.o') else x) for x in l];l.insert(l.index('/build/legacy-startup/full.o')+1,'/build/legacy-startup/local.o');l[l.index('-o')+1]='/build/legacy-startup/full-local'
subprocess.run(prefix+l,check=True)
with (o/'fluid-bounded/local-full.log').open('w') as f:subprocess.run(prefix+['env','JUDAS_ENGINE_ROOT=/source','SDL_AUDIODRIVER=dummy','LIBGL_ALWAYS_SOFTWARE=1','xvfb-run','-a','timeout','45','/build/legacy-startup/full-local','/tmp/legacy/judas_tech_demo.judasproj'],stdout=f,stderr=subprocess.STDOUT,check=True)

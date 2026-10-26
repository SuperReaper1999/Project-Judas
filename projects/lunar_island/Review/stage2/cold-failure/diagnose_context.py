from pathlib import Path
import shlex,subprocess
repo=Path.cwd();target=repo/'build/CMakeFiles/judas.dir';f={}
for line in (target/'flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);f[k]=shlex.split(v)
s=(repo/'src/physics/WorldPersistence.cpp').read_text().replace('#include "WorldInternal.h"','#include "WorldInternal.h"\n#include <fstream>\n#include <iostream>')
s=s.replace('a.Require(context == expectedContext, "joint continuation physical context differs");','''if(context!=expectedContext){std::ofstream("/tmp/lunar-saved-context.bin",std::ios::binary).write(context.data(),context.size());std::ofstream("/tmp/lunar-restored-context.bin",std::ios::binary).write(expectedContext.data(),expectedContext.size());std::cerr<<"CONTEXT joint "<<handle.id<<" bodyA "<<joint.state.settings.bodyA.id<<" bodyB "<<joint.state.settings.bodyB.id<<" sizes "<<context.size()<<" "<<expectedContext.size()<<std::endl;}a.Require(context == expectedContext, "joint continuation physical context differs");''')
Path('/tmp/LunarContextProbe.cpp').write_text(s)
subprocess.run(['/usr/bin/c++',*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-I'+str(repo/'src/physics'),'-c','/tmp/LunarContextProbe.cpp','-o','/tmp/LunarContextProbe.o'],check=True)
a=shlex.split((target/'link.txt').read_text());a=[x for x in a if not x.startswith('-Wl,--dependency-file=')];a=['/tmp/lunar-stage2-observer-final.o' if x.endswith('/src/main.cpp.o') else '/tmp/LunarContextProbe.o' if x.endswith('/src/physics/WorldPersistence.cpp.o') else x for x in a];a.insert(1,'/tmp/LunarContextProbe.o');a[a.index('-o')+1]='/tmp/lunar-stage2-context-probe';subprocess.run(a,cwd=repo/'build',check=True)

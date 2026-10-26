from pathlib import Path
import shlex,subprocess
repo=Path.cwd();target=repo/'build/CMakeFiles/judas.dir';f={}
for line in (target/'flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);f[k]=shlex.split(v)
s=(repo/'src/physics/WorldPersistence.cpp').read_text().replace('#include "WorldInternal.h"','#include "WorldInternal.h"\n#include <fstream>\n#include <iostream>')
s=s.replace('m_comparison->Require(saved == value, "continuation physical context differs");', '\n'.join([
'if(saved != value){SaveArchive before,after;before(saved);after(value);std::ofstream("/tmp/lunar-field-saved.bin",std::ios::binary).write(before.bytes.data(),before.bytes.size());std::ofstream("/tmp/lunar-field-restored.bin",std::ios::binary).write(after.bytes.data(),after.bytes.size());std::cerr<<"FIELD end offset "<<m_comparison->position<<" type "<<typeid(T).name()<<std::endl;}',
'm_comparison->Require(saved == value, "continuation physical context differs");']))
s=s.replace('m_impl->ContinuationJointContext(joint, &comparison);', 'try {m_impl->ContinuationJointContext(joint, &comparison);}catch(...){std::ofstream("/tmp/lunar-saved-context.bin",std::ios::binary).write(context.data(),context.size());std::ofstream("/tmp/lunar-restored-context.bin",std::ios::binary).write(expectedContext.data(),expectedContext.size());std::cerr<<"CONTEXT joint "<<handle.id<<" bodyA "<<joint.state.settings.bodyA.id<<" bodyB "<<joint.state.settings.bodyB.id<<std::endl;throw;}')
Path('/tmp/LunarContextProbe.cpp').write_text(s)
subprocess.run(['/usr/bin/c++',*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-I'+str(repo/'src/physics'),'-c','/tmp/LunarContextProbe.cpp','-o','/tmp/LunarContextProbe.o'],check=True)
a=shlex.split((target/'link.txt').read_text());a=[x for x in a if not x.startswith('-Wl,--dependency-file=')];a=['/tmp/lunar-stage2-observer-final.o' if x.endswith('/src/main.cpp.o') else '/tmp/LunarContextProbe.o' if x.endswith('/src/physics/WorldPersistence.cpp.o') else x for x in a];a.insert(1,'/tmp/LunarContextProbe.o');a[a.index('-o')+1]='/tmp/lunar-stage2-context-probe';subprocess.run(a,cwd=repo/'build',check=True)

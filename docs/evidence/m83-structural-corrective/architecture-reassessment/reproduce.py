#!/usr/bin/env python3
"""Linux-only isolated representation probes; does not build/change the engine.
Requires the recorded research engine build. Outputs compiled binaries outside
tracked source. Prototype JointSolver is linked ahead of the normal archive.
No public API claim, final game validation, commit or push is performed.
"""
import argparse, hashlib, json, shlex, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[4])
p.add_argument('--build',type=Path,default=Path('build'))
p.add_argument('--output',type=Path,required=True)
p.add_argument('--cases',nargs='+',choices=['native-assembly','compound','coarse'],default=['coarse'])
p.add_argument('--plan',action='store_true')
a=p.parse_args();root=a.root.resolve();build=(root/a.build).resolve();source=Path(__file__).resolve().parent
link=build/'CMakeFiles/judas_joint_tests.dir/link.txt'
if not link.is_file():raise SystemExit('Missing native Linux research link command: '+str(link))
base=[x for x in shlex.split(link.read_text()) if not x.startswith('-Wl,--dependency-file=')]
configs={'native-assembly':('NativeAssemblyBenchmark.cpp',None),'compound':('CompoundBenchmark.cpp',None),'coarse':('coarse-sections/Benchmark.cpp','coarse-sections/JointSolver.cpp')}
if a.plan:
 print(json.dumps({'root':str(root),'build':str(build),'cases':a.cases,'engine_rebuilt':False,'output':str(a.output.resolve())},indent=2));raise SystemExit()
out=a.output.resolve()
if out.exists():raise SystemExit('Output must be a fresh directory')
out.mkdir(parents=True);records=[]
for name in a.cases:
 cpp,override=configs[name];commands=[];obj=out/(name+'.o');binary=out/name
 commands.append(['/usr/bin/c++','-O3','-DNDEBUG','-std=gnu++17','-Wall','-Wextra','-I',str(root/'src'),'-c',str(source/cpp),'-o',str(obj)])
 cmd=list(base);i=next(i for i,x in enumerate(cmd) if x.endswith('/JointTests.cpp.o'));cmd[i]=str(obj);cmd[cmd.index('-o')+1]=str(binary)
 inputs=[source/cpp,build/'libjudas_engine.a']
 if override:
  extra=out/(name+'-joint.o');commands.append(['/usr/bin/c++','-O3','-DNDEBUG','-std=gnu++17','-Wall','-Wextra','-I',str(root/'src'),'-c',str(source/override),'-o',str(extra)])
  cmd.insert(cmd.index('libjudas_engine.a'),str(extra));inputs.append(source/override)
 commands.append(cmd)
 with (out/(name+'-build.txt')).open('w') as log:
  for command in commands:subprocess.run(command,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
 with (out/(name+'-results.txt')).open('w') as log:result=subprocess.run([str(binary)],cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=120)
 records.append({'case':name,'exit_code':result.returncode,'commands':commands,'fingerprints':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs+[binary]}})
(out/'RUN.json').write_text(json.dumps(records,indent=2)+'\n')
raise SystemExit(any(r['exit_code'] for r in records))

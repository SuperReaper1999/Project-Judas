from pathlib import Path
import subprocess,shlex,json
root=Path.cwd();build=root/'.cache/2d-stage1-build';out=root/'.cache/2d-stage1-evidence/render-baseline';out.mkdir()
flags={}
for line in (build/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if line.startswith(('CXX_DEFINES =','CXX_INCLUDES =','CXX_FLAGS =')):
  k,v=line.split('=',1);flags[k.strip()]=shlex.split(v)
objects=[]
for name in ('Renderer','WorldPresentation'):
 source=out/(name+'.cpp');source.write_bytes(subprocess.check_output(['git','show','4aca1c3ff70e385dd5cc81dc36983167faa0b36c:src/'+name+'.cpp']))
 obj=out/(name+'.o');objects.append(str(obj))
 cmd=['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(source),'-o',str(obj)]
 print(shlex.join(cmd),flush=True);subprocess.run(cmd,check=True)
command=shlex.split((build/'CMakeFiles/judas_runtime_render_tests.dir/link.txt').read_text())
i=command.index('libjudas_engine.a');command[i:i]=objects
command[command.index('-o')+1]=str(out/'baseline-render-tests')
print(shlex.join(command),flush=True);subprocess.run(command,cwd=build,check=True)

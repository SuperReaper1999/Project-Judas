from pathlib import Path
import shlex,subprocess,sys
root=Path.cwd();out=root/'.cache/prealpha-pass4/ccd'/sys.argv[1];source=root/'.cache/prealpha-pass4/ccd'/sys.argv[2];name=source.stem;flags=(root/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();args=[]
for key in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:args+=shlex.split(next(x.split('=',1)[1] for x in flags if x.startswith(key+' =')))
cmd=['c++','-I',str(out/'sources'),*args,'-I',str(out/'sources'),'-I',str(root/'tests'),str(source),'-c','-o',str(out/(name+'.o'))]
with (out/(name+'.build.log')).open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
if r.returncode:raise SystemExit(r.returncode)
link=shlex.split((root/'build/CMakeFiles/judas_impact_mesh_cost_tests.dir/link.txt').read_text());libs=link[link.index('libjudas_engine.a')+1:];libs=[str(root/'build'/x) if x.endswith('.a') and not x.startswith('/') else x for x in libs]
cmd=['c++','-O3','-DNDEBUG',str(out/(name+'.o')),str(out/'PhysicsWorld.cpp.o'),str(out/'CollisionGeometry.cpp.o'),'-o',str(out/name),str(root/'.cache/prealpha-pass4/ccd/accepted-engine.a'),*libs]
with (out/(name+'.link.log')).open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
raise SystemExit(r.returncode)

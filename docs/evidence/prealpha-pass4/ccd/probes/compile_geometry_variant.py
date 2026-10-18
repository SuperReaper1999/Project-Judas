from pathlib import Path
import shlex,subprocess,sys
root=Path.cwd();out=root/'.cache/prealpha-pass4/ccd'/sys.argv[1];flags=(root/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();args=[]
for key in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:args+=shlex.split(next(x.split('=',1)[1] for x in flags if x.startswith(key+' =')))
with (out/'CollisionGeometry.cpp.build.log').open('w') as f:r=subprocess.run(['c++',*args,'-I',str(root/'src'),'-c',str(out/'sources/CollisionGeometry.cpp'),'-o',str(out/'CollisionGeometry.cpp.o')],stdout=f,stderr=subprocess.STDOUT)
raise SystemExit(r.returncode)

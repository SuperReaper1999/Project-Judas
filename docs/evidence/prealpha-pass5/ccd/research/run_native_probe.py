from pathlib import Path
import subprocess,shlex,json,datetime,time,hashlib
R=Path.cwd();D=R/'.cache/prealpha-pass5/ccd/native-sat-v2';D.mkdir(exist_ok=True);S=R/'.cache/prealpha-pass5/ccd/union-v3/sources';B=R/'.cache/prealpha-pass5/baseline';C=R/'.cache/prealpha-pass5/ccd'
for f in ['NativeSATProbe.cpp','NativeSATContinuity.h','MeshContinuityProbe.h']:(D/f).write_bytes((C/f).read_bytes())
rec={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'diagnostic on exact captured originalbox5 segment, not a new acceptance test','sources':{f.name:hashlib.sha256(f.read_bytes()).hexdigest()for f in D.iterdir()if f.suffix in ['.h','.cpp']},'commands':[]}
def run(name,args,cwd=R):
 t=time.monotonic()
 with (D/(name+'.log')).open('w') as f:r=subprocess.run(args,cwd=cwd,stdout=f,stderr=subprocess.STDOUT,timeout=60)
 rec['commands'].append({'name':name,'argv':args,'cwd':str(cwd),'exit':r.returncode,'seconds':time.monotonic()-t});(D/'BUILD.json').write_text(json.dumps(rec,indent=2));print(name,r.returncode,flush=True);return r.returncode
if run('compile',['c++','-std=c++17','-O3','-DNDEBUG','-I'+str(D),'-I'+str(S),'-I'+str(R/'src'),'-c',str(D/'NativeSATProbe.cpp'),'-o',str(D/'probe.o')]):raise SystemExit(1)
link=shlex.split((R/'build/CMakeFiles/judas_collision_geometry_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=') and not (x.endswith('.o') and 'judas_collision_geometry_tests.dir' in x)];link[link.index('-o')+1]=str(D/'probe');link[link.index('libjudas_engine.a')]=str(B/'libjudas_engine.a');link=[str(B/'libjudas_build_identity.a') if x=='libjudas_build_identity.a' else x for x in link];link[1:1]=[str(D/'probe.o'),str(R/'.cache/prealpha-pass5/ccd/union-v3/CollisionGeometry.cpp.o')]
if run('link',link,R/'build'):raise SystemExit(1)
run('probe',[str(D/'probe')])

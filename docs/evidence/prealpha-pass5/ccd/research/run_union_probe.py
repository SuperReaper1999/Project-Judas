from pathlib import Path
import subprocess,shlex,json,datetime,time
R=Path.cwd();D=R/'.cache/prealpha-pass5/ccd/union-v3';S=D/'sources';B=R/'.cache/prealpha-pass5/baseline';rec={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'commands':[]}
def run(name,args,cwd=R):
 t=time.monotonic()
 with (D/(name+'.log')).open('w') as f:r=subprocess.run(args,cwd=cwd,stdout=f,stderr=subprocess.STDOUT,timeout=60)
 rec['commands'].append({'name':name,'argv':args,'cwd':str(cwd),'exit':r.returncode,'seconds':time.monotonic()-t});(D/'UNION_PROBE.json').write_text(json.dumps(rec,indent=2));print(name,r.returncode,flush=True);return r.returncode
if run('compile-union-probe',['c++','-std=c++17','-O3','-DNDEBUG','-I'+str(S),'-I'+str(R/'src'),'-c',str(D/'UnionProbe.cpp'),'-o',str(D/'union-probe.o')]):raise SystemExit(1)
link=shlex.split((R/'build/CMakeFiles/judas_collision_geometry_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=') and not (x.endswith('.o') and 'judas_collision_geometry_tests.dir' in x)];link[link.index('-o')+1]=str(D/'union-probe');link[link.index('libjudas_engine.a')]=str(B/'libjudas_engine.a');link=[str(B/'libjudas_build_identity.a') if x=='libjudas_build_identity.a' else x for x in link];link[1:1]=[str(D/'union-probe.o'),str(R/'.cache/prealpha-pass5/ccd/union-v2/CollisionGeometry.cpp.o')]
if run('link-union-probe',link,R/'build'):raise SystemExit(1)
run('union-probe',[str(D/'union-probe')])

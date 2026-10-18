from pathlib import Path
import datetime,hashlib,json,shlex,subprocess,sys,time
R=Path.cwd();D=R/'.cache/prealpha-pass5/ccd'/sys.argv[1];S=D/'sources';B=R/'.cache/prealpha-pass5/baseline'
rec={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'baseline':'4c141e9bb6c68a50c63dec263dcec398200328ad','scope':'cache-only coherent private-world and finite-contact-union research','sources':{str(f.relative_to(S)):hashlib.sha256(f.read_bytes()).hexdigest() for f in S.rglob('*') if f.is_file()},'commands':[]}
def run(name,argv,cwd=R,timeout=120):
 start=time.monotonic()
 with (D/(name+'.log')).open('w') as log:
  try: code=subprocess.run(argv,cwd=cwd,stdout=log,stderr=subprocess.STDOUT,timeout=timeout).returncode
  except subprocess.TimeoutExpired:code='timeout'+str(timeout)
 rec['commands'].append({'name':name,'argv':list(map(str,argv)),'cwd':str(cwd),'exit':code,'seconds':time.monotonic()-start});(D/'BUILD.json').write_text(json.dumps(rec,indent=2)+'\n');print(name,code,flush=True);return code
flags=(R/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();args=['-I'+str(S),'-I'+str(S/'physics')]
for key in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:args+=shlex.split(next(x.split('=',1)[1] for x in flags if x.startswith(key+' =')))
for name in ([] if '--reuse' in sys.argv else ['UnifiedWorld.cpp','CollisionGeometry.cpp']):
 if run('compile-'+name,['/usr/bin/c++',*args,'-c',str(S/name),'-o',str(D/(name+'.o'))]):raise SystemExit(1)
# All private Impl users are compiled together. Remove all accepted world objects
# so there can be no silent mix of distinct private layouts.
archive=D/'engine-without-world.a';archive.write_bytes((B/'libjudas_engine.a').read_bytes())
listing=subprocess.check_output(['ar','t',str(archive)],text=True).splitlines();remove=[x for x in listing if x=='PhysicsWorld.cpp.o' or (x.startswith('World') and x in {f.name+'.o' for f in (S/'physics').glob('World*.cpp')}) or x=='CollisionGeometry.cpp.o']
assert len(remove)==13 and remove.count('WorldPersistence.cpp.o')==2,remove
# The archive has two same-basename WorldPersistence members. Preserve public
# world/save orchestration (first) and replace only physics-private second.
if run('archive-private-persistence',['ar','dN','2',str(archive),'WorldPersistence.cpp.o']):raise SystemExit(1)
if run('archive',['ar','d',str(archive),*[x for x in remove if x!='WorldPersistence.cpp.o']]):raise SystemExit(1)
link=shlex.split((R/'build/CMakeFiles/judas_impact_mesh_cost_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
link[link.index('-o')+1]=str(D/'mesh');link[link.index('libjudas_engine.a')]=str(archive)
link=[str(B/'libjudas_build_identity.a') if x=='libjudas_build_identity.a' else x for x in link]
link[1:1]=[str(D/'UnifiedWorld.cpp.o'),str(D/'CollisionGeometry.cpp.o')]
if run('link-mesh',link,R/'build'):raise SystemExit(1)
# Original engine test source/object and settings remain unchanged.
run('continuation',[str(D/'mesh'),'1','trough'],timeout=90)
run('entry',[str(D/'mesh'),'1','trough','1.2','entry'],timeout=90)
probe=R/'.cache/prealpha-pass4/ccd/MinimalContinuationRepro.cpp'
if run('compile-minimal',['/usr/bin/c++','-std=c++17','-O3','-DNDEBUG','-I'+str(R/'src'),'-I'+str(R/'tests'),'-c',str(probe),'-o',str(D/'minimal.o')]):raise SystemExit(1)
mini=link[:];mini=[x for x in mini if not (x.endswith('.o') and 'judas_impact_mesh_cost_tests.dir' in x)];mini[1:1]=[str(D/'minimal.o')];mini[mini.index('-o')+1]=str(D/'minimal')
if run('link-minimal',mini,R/'build'):raise SystemExit(1)
run('minimal',[str(D/'minimal')],timeout=30)

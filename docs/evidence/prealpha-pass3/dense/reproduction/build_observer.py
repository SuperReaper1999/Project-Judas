from pathlib import Path
import shlex,subprocess,shutil,json,hashlib,time,datetime
r=Path.cwd();o=r/'.cache/prealpha-pass3/dense';b=o/'observer';b.mkdir(exist_ok=True);frozen=r/'.cache/prealpha-pass3/baseline/bin';records=[]
def run(cmd,log):
 start=time.monotonic()
 with log.open('w')as stream:subprocess.run(list(map(str,cmd)),cwd=r/'build',stdout=stream,stderr=subprocess.STDOUT,check=True)
 records.append({'argv':list(map(str,cmd)),'seconds':time.monotonic()-start,'log':str(log)});print('built',log.name,flush=True)
lines=(r/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();flags=[]
for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(next(x.split('=',1)[1]for x in lines if x.startswith(n+' =')))
src=o/'PhysicsWorld.observer-baseline.cpp';obj=b/'PhysicsWorld.cpp.o';run(['/usr/bin/c++',*flags,'-c',src,'-o',obj],b/'build.log')
archive=b/'libjudas_engine.a';shutil.copyfile(frozen/'libjudas_engine.a',archive);run(['ar','r',archive,obj],b/'archive.log')
args=shlex.split((r/'build/CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());objects=[str((r/'build'/x).resolve())for x in args if x.endswith('.o')];libs=args[args.index('libjudas_engine.a')+1:];libs=[str((r/'build'/x).resolve())if x.endswith('.a')and not x.startswith('/')else x for x in libs];binary=b/'live';run(['/usr/bin/c++','-O3','-DNDEBUG',*objects,'-o',binary,archive,*libs],b/'link.log')
(b/'BUILD.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'commands':records,'sources':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest()for p in [src,r/'src/Contacts.cpp',o/'Contacts.baseline.cpp']},'baselineArchive':hashlib.sha256((frozen/'libjudas_engine.a').read_bytes()).hexdigest(),'binary':hashlib.sha256(binary.read_bytes()).hexdigest(),'note':'Cache-only accepted-baseline observer, not production timings. Contacts.cpp from frozen baseline archive; tracked current Contacts.cpp is recorded only to disambiguate checkout.'},indent=2)+'\n')

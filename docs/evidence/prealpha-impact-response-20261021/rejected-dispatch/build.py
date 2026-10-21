from pathlib import Path
import subprocess,shlex,json,hashlib,shutil,datetime
root=Path('/home/conner/Documents/GitHub/Project-Judas');o=Path(__file__).resolve().parent;b=root/'build';base=root/'.cache/event-architecture-20261021/final-snapshot';receipt={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'baseCoreSha256':hashlib.sha256((base/'libjudas_engine.a').read_bytes()).hexdigest(),'commands':[]};flags={}
for line in (b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
def run(args,cwd=root):
 receipt['commands'].append({'argv':list(map(str,args)),'cwd':str(cwd)});p=subprocess.run(list(map(str,args)),cwd=cwd);assert p.returncode==0,p.returncode
for n in ['WorldImpactEvents']:
 run(['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(root),'-I'+str(root/'.cache/dense-root-cause-20261020/include/src/physics'),'-I'+str(root/'.cache/dense-root-cause-20261020/include/src'),'-I'+str(o),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-MMD','-MF',str(o/(n+'.d')),'-c',o/(n+'.cpp'),'-o',o/(n+'.cpp.o')])
archive=o/'libjudas_engine.a';shutil.copy2(base/'libjudas_engine.a',archive);run(['ar','r',archive,o/'WorldImpactEvents.cpp.o'])
link=shlex.split((b/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(o/'judas_physics_stress_live_benchmark');link=[str(archive) if x=='libjudas_engine.a' else x for x in link];run(link,b)
receipt['artifacts']={p.name:{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size} for p in o.iterdir() if p.is_file() and p.suffix not in ['.a']};(o/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Observer built, no production build modified')

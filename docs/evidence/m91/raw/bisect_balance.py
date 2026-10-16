from pathlib import Path
import subprocess,shlex,hashlib,json,time
r=Path('/home/conner/Documents/GitHub/Project-Judas');b=r/'build';out=r/'.cache/m91/balance-bisect';out.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
flags={}
for line in(b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
link=shlex.split((b/'CMakeFiles/judas_physical_balance_controller_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
original=(r/'src/PhysicsWorld.cpp').read_text();start=original.index('            // A velocity-changing island invalidates');end=original.index('            std::vector<unsigned> changed;',start)
no_history=original[:start]+'''            contactCache.erase(std::remove_if(contactCache.begin(),contactCache.end(),
                [&](const CachedContact& c){return included[c.key.slotA] || included[c.key.slotB];}),contactCache.end());
'''+original[end:]
variants={'baseline':None,'no-certificate':original.replace('!SupportContinuationSafe(pair,now,dt)','true'),'no-history':no_history}
receipt={'current_source_sha256':sha(r/'src/PhysicsWorld.cpp'),'baseline_archive_sha256':sha(r/'.cache/m91/baseline/libjudas_engine.a'),'runs':[]}
for name,source in variants.items():
 d=out/name;d.mkdir();archive=r/'.cache/m91/baseline/libjudas_engine.a'
 if source is not None:
  path=d/'PhysicsWorld.cpp';path.write_text(source);obj=d/'PhysicsWorld.cpp.o';compile=['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(path),'-o',str(obj)]
  with(d/'build.log').open('w')as log:subprocess.run(compile,cwd=b,stdout=log,stderr=subprocess.STDOUT,check=True)
  archive=d/'libjudas_engine.a';archive.write_bytes((b/'libjudas_engine.a').read_bytes());subprocess.run(['ar','r',str(archive),str(obj)],check=True)
 cmd=[str(archive) if x=='libjudas_engine.a'else x for x in link];cmd[cmd.index('-o')+1]=str(d/'balance');subprocess.run(cmd,cwd=b,check=True)
 with(d/'run.log').open('w')as log:run=subprocess.run([str(d/'balance')],cwd=r,stdout=log,stderr=subprocess.STDOUT,timeout=120)
 receipt['runs'].append({'name':name,'exit':run.returncode,'archive_sha256':sha(archive),'binary_sha256':sha(d/'balance')});(out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');print(name,run.returncode,flush=True)

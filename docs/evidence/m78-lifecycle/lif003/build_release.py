from pathlib import Path
import hashlib,json,shlex,shutil,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';out=r/'.cache/m78-lifecycle/lif003-build';ev=r/'docs/evidence/m78-lifecycle/lif003/build';out.mkdir(parents=True);ev.mkdir(parents=True)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();old=r/'.cache/m78-lifecycle/final-build-v2/libjudas_engine.a';assert h(old)=='e2e4aec5ed13c6a6e5eb4db0f528682671ad879ae27080665699d8b61681618f'
f={}
for line in(base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);f[k]=shlex.split(v)
rows=[];objs=[]
for s in ['src/WorldCharacter.cpp','src/RuntimeWorld.cpp']:
 obj=out/(Path(s).name+'.o');cmd=['c++',*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-c',str(r/s),'-o',str(obj)];t=time.monotonic();p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(ev/(Path(s).stem+'.log')).write_bytes(p.stdout);rows.append(dict(source=s,sourceSHA256=h(r/s),command=cmd,exit=p.returncode,seconds=time.monotonic()-t));print(s,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.stdout.decode()[-3000:])
 objs.append(obj)
a=out/'libjudas_engine.a';shutil.copyfile(old,a);subprocess.run(['ar','r',str(a),*map(str,objs)],check=True)
(ev/'BUILD.json').write_text(json.dumps(dict(priorArchiveSHA256=h(old),archive=dict(path=str(a.relative_to(r)),sha256=h(a)),sources=rows,scope='Two production translation units only, existing Release flags and immutable prior archive.'),indent=2)+'\n')

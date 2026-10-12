from pathlib import Path
import hashlib,json,os,shlex,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';out=r/'.cache/m78-lifecycle/lif003-public';ev=r/'docs/evidence/m78-lifecycle/lif003/public-audit';out.mkdir(parents=True,exist_ok=False);ev.mkdir(exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();source=r/'tests/ConsumerLifecycleAuditTests.cpp';archive=r/'.cache/m78-lifecycle/lif003-build/libjudas_engine.a';rows=[];identity=dict(sourceSHA256=h(source),archiveSHA256=h(archive))
f={}
for line in(base/'CMakeFiles/judas_save_application_tests.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);f[k]=shlex.split(v)
obj=out/'public.o';exe=out/'public';cmd=shlex.split((base/'CMakeFiles/judas_save_application_tests.dir/link.txt').read_text());cmd=[str(obj)if x.endswith('/SaveApplicationTests.cpp.o')else x.replace('CMakeFiles/judas_save_application_tests.dir/src/','CMakeFiles/judas.dir/src/')if x.endswith('.cpp.o')else str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];cmd[cmd.index('-o')+1]=str(exe)
identity['existingLinkInputs']={x:h(base/x)for x in cmd if (base/x).is_file()};(ev/'compiled-source.cpp').write_bytes(source.read_bytes())
def run(name,cmd,cwd=r):
 t=time.monotonic();p=subprocess.run(cmd,cwd=cwd,env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data')),stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=90);(ev/(name+'.stdout.log')).write_bytes(p.stdout);(ev/(name+'.stderr.log')).write_bytes(p.stderr);rows.append(dict(name=name,command=cmd,exit=p.returncode,seconds=time.monotonic()-t));(ev/'RECEIPTS.json').write_text(json.dumps(dict(identity=identity,results=rows),indent=2)+'\n');print(name,p.returncode,round(rows[-1]['seconds'],3),flush=True)
 if p.returncode:raise SystemExit((p.stderr+p.stdout).decode(errors='replace')[-4000:])
import shutil
prior=r/'.cache/m78-lifecycle/public-after-01/public.o';shutil.copyfile(prior,obj);identity['reusedObjectSHA256']=h(obj)
run('link',cmd,base);identity.update(binarySHA256=h(exe),objectSHA256=h(obj));run('run',[str(exe),str(out/'fixture')])

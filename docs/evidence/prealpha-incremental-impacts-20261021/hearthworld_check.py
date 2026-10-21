from pathlib import Path
import os,json,subprocess,time,hashlib
r=Path.cwd();o=r/'.cache/contact-redesign-20261021/hearthworld';o.mkdir(exist_ok=True);env=os.environ.copy();env.update(SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(o/'userdata'),XDG_CONFIG_HOME=str(o/'config'),XDG_CACHE_HOME=str(o/'cache'));results=[];binary=r/'build/judas_hearthworld_application_tests'
for mode in['write','read']:
 cmd=[str(binary),str(r/'projects/hearthworld/hearthworld.judasproj'),mode,str(o/'game')];t=time.monotonic()
 with(o/(mode+'.log')).open('w')as f:c=subprocess.run(cmd,cwd=r,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=240).returncode
 results.append({'mode':mode,'argv':cmd,'exit':c,'seconds':time.monotonic()-t,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()});(o/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(mode,c,flush=True)
 if c:raise SystemExit(c)

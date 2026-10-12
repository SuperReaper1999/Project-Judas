#!/usr/bin/env python3
"""Final closure identical four-case Linux workload, after scoped query-publication repair; no simultaneous native workload."""
from pathlib import Path
import datetime,hashlib,json,os,statistics,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';build=r/'.cache/m78-closure/final-build';out=r/'.cache/m78-closure/final-performance';ev=r/'docs/evidence/m78-closure/final-performance'
out.mkdir(parents=True,exist_ok=False);ev.mkdir(parents=True,exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
old=json.loads((r/'docs/evidence/m78-continuation/midpoint-final-concave-build.json').read_text());source=r/'docs/evidence/m78-followup/probes/concave_final.cpp';assert h(source)==old['sourceSHA256']
identity=json.loads((r/'docs/evidence/m78-closure/final-build/BUILD.json').read_text());archive=build/'libjudas_engine.a';assert h(archive)==identity['archive']['sha256']
for p,v in identity['sourceSHA256'].items():assert h(r/p)==v,p
cmd=old['builds'][1]['command'];cmd[cmd.index('-o')+1]=str(out/'ragdoll-performance');cmd[cmd.index(str(base/'libjudas_engine.a'))]=str(archive)
start=time.perf_counter();p=subprocess.run(cmd,cwd=base,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(ev/'link.log').write_text(p.stdout);assert p.returncode==0
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config'))
load=Path('/proc/loadavg').read_text();start=time.perf_counter();exe=out/'ragdoll-performance';run=[str(exe),str(out/'raw')];p=subprocess.run(run,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);(ev/'run.log').write_text(p.stdout)
raw=out/'raw';[__import__('shutil').copyfile(x,ev/x.name)for x in raw.iterdir()]
rows=[json.loads(x)for x in p.stdout.splitlines()if x.startswith('{"case"')]
import csv
mesh=list(csv.DictReader((raw/'mesh-4.csv').open()));slow=[x for x in mesh if float(x['wall_ms'])>16.67]
result=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope=__doc__,command=run,exitCode=p.returncode,seconds=time.perf_counter()-start,loadAverageBefore=load.strip(),sourceSHA256=h(source),archiveSHA256=h(archive),executableSHA256=h(exe),sourceHashes=identity['sourceSHA256'],linkCommand=cmd,rows=rows,fourRigStepsOver16_67=len(slow),fourRigTotalSteps=len(mesh),rawHashes={x.name:h(x)for x in ev.iterdir()if x.name.endswith(('.csv','-profile.json'))},limits='Linux host performance only; rendering is offscreen software. Original VM timing remains nonblocking functional evidence.')
(ev/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(exitCode=p.returncode,rows=rows,over16_67=len(slow)),indent=2));raise SystemExit(p.returncode)

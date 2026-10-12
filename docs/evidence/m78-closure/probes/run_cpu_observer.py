#!/usr/bin/env python3
"""Bounded timing diagnosis: exact original four workloads plus observational thread-CPU clock; serial prior/final archives."""
from pathlib import Path
import hashlib,json,os,subprocess,time,csv
r=Path.cwd();base=r/'.cache/m78-followup/release';out=r/'.cache/m78-closure/cpu-observer';ev=r/'docs/evidence/m78-closure/cpu-observer'
out.mkdir(parents=True,exist_ok=False);ev.mkdir(parents=True,exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
old=json.loads((r/'docs/evidence/m78-continuation/midpoint-final-concave-build.json').read_text());source=r/'docs/evidence/m78-closure/probes/concave_cpu_observer.cpp';obj=out/'observer.o';receipts=[]
def run(name,cmd,cwd=r):
 start=time.perf_counter();p=subprocess.run(cmd,cwd=cwd,env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config')),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);log=ev/(name+'.log');log.write_text(p.stdout);rec=dict(name=name,command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-start,logSHA256=h(log));receipts.append(rec);(ev/'RECEIPTS.json').write_text(json.dumps(dict(scope=__doc__,sourceSHA256=h(source),results=receipts),indent=2)+'\n');print(name,p.returncode,flush=True);assert p.returncode==0;return p.stdout
cmd=old['builds'][0]['command'].copy();cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(obj);run('compile',cmd)
summary=[]
for label,archive in [('prior',r/'.cache/m78-cold-boundary/final-after-static-build/libjudas_engine.a'),('final',r/'.cache/m78-closure/final-build/libjudas_engine.a')]:
 cmd=old['builds'][1]['command'].copy();cmd=[str(obj)if x.endswith('/midpoint-final-concave.o') else str(archive)if x.endswith('/libjudas_engine.a')else x for x in cmd];exe=out/label;cmd[cmd.index('-o')+1]=str(exe);run(label+'-link',cmd,base);raw=ev/label;text=run(label+'-run',[str(exe),str(raw)])
 rows=list(csv.DictReader((raw/'mesh-4.csv').open()));wall=sorted(float(x['wall_ms'])for x in rows);cpu=sorted(float(x['thread_cpu_ms'])for x in rows)
 rec=dict(label=label,archiveSHA256=h(archive),binarySHA256=h(exe),wallMedianMs=wall[450],wallP95Ms=wall[854],wallMaxMs=wall[-1],threadCpuMedianMs=cpu[450],threadCpuP95Ms=cpu[854],threadCpuMaxMs=cpu[-1],wallStepsOver16_67=sum(v>16.67 for v in wall),aggregatePhysical=dict(contactRows=sum(int(x['contacts'])for x in rows),ccdIterations=sum(int(x['ccd_iterations'])for x in rows),ccdLimit=sum(int(x['ccd_limit'])for x in rows)),rawSHA256={x.name:h(x)for x in raw.iterdir()},limit='Observation includes per-step thread CPU clock overhead, identical in both serial variants. CPU time is not the frame-time acceptance gate. External host load uncontrolled; no scheduling/affinity/resource changes.')
 summary.append(rec);(ev/'RESULTS.json').write_text(json.dumps(dict(scope=__doc__,results=summary),indent=2)+'\n');print(json.dumps(rec),flush=True)

#!/usr/bin/env python3
"""Run preserved ordinary workloads using explicitly frozen binaries.
No source changes, generated scenes, solver controls, acceptance retuning or timing gates.
The root coordinator serializes this script with builds/other CPU-heavy work.
"""
from pathlib import Path
import argparse,csv,datetime,hashlib,json,os,statistics,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--live',type=Path,required=True);p.add_argument('--scaling',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--skip-dense',action='store_true');p.add_argument('--only-dense',action='store_true');p.add_argument('--root',type=Path,default=Path.cwd());a=p.parse_args();root=a.root.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
def sha(f):return hashlib.sha256(f.read_bytes()).hexdigest()
rec={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'root':str(root),'git_head_at_start':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'binaries':{str(x.resolve()):sha(x) for x in [a.live,a.scaling] if x},'fixture':{'path':'projects/physics_stress_lab/Scenes/room1024.judas','sha256':sha(root/'projects/physics_stress_lab/Scenes/room1024.judas')},'results':[],'environment':{k:os.getenv(k) for k in ['DISPLAY','WAYLAND_DISPLAY','SDL_VIDEODRIVER','XDG_SESSION_TYPE']},'notes':['Exact unchanged workload/physics args; no parallel owned CPU build or run intended.','Timing is host diagnostic, not a universal real-time bound. Raw distributions/physical results retained.']}
def save():(out/'RESULTS.json').write_text(json.dumps(rec,indent=2)+'\n')
def run(name,cmd,limit):
 entry={'name':name,'argv':list(map(str,cmd)),'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'timeout_seconds':limit};start=time.monotonic()
 with(out/(name+'.log')).open('w')as log:
  try:r=subprocess.run(entry['argv'],cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=limit);entry['exit']=r.returncode
  except subprocess.TimeoutExpired:entry['exit']='timeout'
 entry['wall_seconds']=time.monotonic()-start;rec['results'].append(entry);save();print(name,entry['exit'],entry['wall_seconds'],flush=True)
 if name=='dense270':
  f=out/name/'steps.csv';rows=list(csv.DictReader(f.open()))if f.exists()else[];times=sorted(float(x['fixed_ms'])for x in rows)
  if times:entry.update({'completed_steps':len(rows),'fixed_ms':{'median':statistics.median(times),'p95_floor':times[int(.95*(len(times)-1))],'maximum':times[-1]},'final_digest':rows[-1]['trajectory_digest'],'worst_step':max(rows,key=lambda x:float(x['fixed_ms']))})
  quality=out/name/'quality.json'
  if quality.exists():entry['endpoint_quality']=json.loads(quality.read_text())
 else:
  f=out/name/'summary.json'
  if f.exists():entry['summary']=json.loads(f.read_text())
 save()
save()
if not a.skip_dense:run('dense270',[a.live.resolve(),root/rec['fixture']['path'],out/'dense270','105','270','0','0','1','0.016666667','1','0','1'],150)
if not a.only_dense:
 if not a.scaling:raise SystemExit('--scaling required for control workloads')
 for workload,count,warm,steps in [('free',1024,0,120),('pile',256,60,120),('machines_active',256,30,120),('machines_sleeping',256,240,120)]:
  run(f'{workload}{count}',[a.scaling.resolve(),'--workload',workload,'--count',str(count),'--warm',str(warm),'--steps',str(steps),'--output',out/f'{workload}{count}','--profile','1'],45)
rec['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();save()

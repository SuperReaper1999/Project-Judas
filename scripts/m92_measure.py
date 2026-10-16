#!/usr/bin/env python3
"""Alternate matching baseline/candidate native GL workloads. Never stops other apps."""
import argparse,datetime,hashlib,json,os,pathlib,subprocess,time
R=pathlib.Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--baseline',default='.cache/m92/baseline');p.add_argument('--output',default='.cache/m92/measure');p.add_argument('--rounds',type=int,default=2);a=p.parse_args();out=(R/a.output).resolve();out.mkdir(parents=True,exist_ok=True);base=(R/a.baseline).resolve();receipt={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'baselineCommit':'61a9df6acaa102c0a9fcf1006866a01fc3b33a89','resolution':[960,540],'samples':60,'warmup':12,'sharedHost':False,'otherApplications':'Operator authorized closing exact PID 128249 after initial shared-load observation. Desktop applications remain; before/after process and utilization samples are retained.','runs':[]}
def observe():
 values={}
 for key,cmd in [('gpu',['nvidia-smi','--query-gpu=name,driver_version,utilization.gpu,memory.used','--format=csv']),('games',['pgrep','-a','-x','judas'])]:
  r=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);values[key]=r.stdout
 return values
for round in range(a.rounds):
 order=['baseline','candidate'] if round%2==0 else ['candidate','baseline']
 for stage in order:
  for name,binary in [('objects',base/'rendering_scale_cpu' if stage=='baseline' else R/'build/judas_rendering_scale_tests'),('shadows',base/'shadow_scale_cpu' if stage=='baseline' else R/'build/judas_rendering_shadow_scale_tests')]:
   target=out/f'{round}-{stage}-{name}';target.mkdir(exist_ok=True);row={'stage':stage,'round':round,'name':name,'binary':str(binary),'sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'before':observe()};start=time.monotonic()
   with (target/'stdout.log').open('w') as f:r=subprocess.run(['/usr/bin/time','-v','-o',str(target/'memory.txt'),str(binary),str(target)],cwd=R,stdout=f,stderr=subprocess.STDOUT,timeout=240)
   row.update(exit=r.returncode,seconds=time.monotonic()-start,after=observe(),path=str(target.relative_to(R)));receipt['runs'].append(row);(out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');print(stage,name,r.returncode,round,flush=True)
   if r.returncode:raise SystemExit(r.returncode)

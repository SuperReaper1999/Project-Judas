#!/usr/bin/env python3
"""Serial matching completed-frame observations; never stops other applications."""
from pathlib import Path
import datetime,hashlib,json,os,subprocess,time
R=Path(__file__).resolve().parents[1];out=R/'.cache/m93/measure';out.mkdir(parents=True,exist_ok=True)
def observe():
 result={}
 for name,cmd in [('gpu',['nvidia-smi','--query-gpu=name,driver_version,utilization.gpu,memory.used','--format=csv']),('games',['pgrep','-a','-x','judas']),('cpu',['lscpu'])]:
  p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);result[name]=p.stdout
 return result
receipt={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'baseline':'bb3dbc402629a60c22738da8520e5017998709f1','resolution':[960,540],'secondary':[480,270],'samples':60,'warmup':12,'timing':'completed frame wall time (glFinish), not isolated CPU submission or pure GPU time','cache':'fresh process each run; OS cache not flushed','runs':[]}
for round in range(2):
 modes=['source-baseline','source-candidate','cooked-full','cooked-lod'];modes=modes if round==0 else list(reversed(modes))
 for mode in modes:
  binary=R/('.cache/m93/baseline/asset_benchmark_final' if mode=='source-baseline' else 'build/judas_asset_pipeline_benchmark');target=out/f'{round}-{mode}';target.mkdir(exist_ok=True)
  command=[str(binary),str(R/'.cache/m93/fixtures'),str(target),'.obj' if mode.startswith('source') else '.judasmesh','full' if mode=='cooked-full' else 'auto'];env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')};env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy')
  row={'mode':mode,'round':round,'command':command,'binarySha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'before':observe()};start=time.monotonic()
  with (target/'stdout.log').open('w') as f:p=subprocess.run(['/usr/bin/time','-v','-o',str(target/'memory.txt'),*command],cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
  row.update(exit=p.returncode,seconds=time.monotonic()-start,after=observe());receipt['runs'].append(row);(out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');print(round,mode,p.returncode,flush=True)
  if p.returncode:raise SystemExit(p.returncode)

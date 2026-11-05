from pathlib import Path
import os,subprocess,json,time,hashlib
r=Path.cwd();out=r/'.cache/renderer-v2/final-matched';out.mkdir(exist_ok=True)
bins={'A':r/'.cache/renderer-v2/baseline/renderer-benchmark-v3','B':r/'build/judas_renderer_v2_benchmark'}
cases=[('mixed',2048,1,8,4000),('repeated',4096,2,0,4000),('sprites',4096,1,0,1500),('particles',100000,2,0,1500),('lights',256,1,128,1500)]
meta={'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'order':'ABBA per workload','binaries':{k:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for k,p in bins.items()},'observer_sha256':hashlib.sha256((r/'tests/RendererV2Benchmark.cpp').read_bytes()).hexdigest(),'runs':[]}
(out/'RUNS.json').write_text(json.dumps(meta,indent=2)+'\n')
env={**os.environ,'SDL_VIDEODRIVER':'x11','SDL_AUDIODRIVER':'dummy'}
for case,n,views,lights,frames in cases:
 for i,kind in enumerate('ABBA'):
  name=f'{case}-{i}-{kind}';cmd=[str(bins[kind]),'--case',case,'--objects',str(n),'--views',str(views),'--lights',str(lights),'--frames',str(frames),'--warmup','120','--out',str(out/name),'--require-gpu'];started=time.time()
  with (out/(name+'.log')).open('w') as log:p=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240)
  row={'case':case,'iteration':i,'binary':kind,'command':cmd,'exit':p.returncode,'elapsed_seconds':time.time()-started};meta['runs'].append(row)
  if p.returncode==0:
   d=json.loads((out/name/'RESULTS.json').read_text());row['cpu']=d['summary']['cpu_submission_ms'];row['gpu']=d['summary']['gpu_ms'];row['rss']=d['process_peak_rss_bytes']
  print(row,flush=True);(out/'RUNS.json').write_text(json.dumps(meta,indent=2)+'\n')
  if p.returncode:raise SystemExit(p.returncode)

from pathlib import Path
import subprocess,json,hashlib,time,datetime
root=Path.cwd();out=root/'.cache/m91/scaling-verified-repeated';out.mkdir(exist_ok=False)
bins={'before':root/'.cache/m91/baseline/judas_physics_scaling_benchmarks','after':root/'build/judas_physics_scaling_benchmarks'};sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();fp={k:sha(p)for k,p in bins.items()};record={'utc_start':datetime.datetime.now(datetime.timezone.utc).isoformat(),'PhysicsWorld_sha256':sha(root/'src/PhysicsWorld.cpp'),'binary_sha256':fp,'runs':[]}
for repeat in range(3):
 for workload in ['free','pile','mesh','machines_sleeping']:
  for label in (['before','after']if repeat%2==0 else['after','before']):
   name=f'{label}-{workload}-r{repeat}';dest=out/name;cmd=[str(bins[label]),'--workload',workload,'--count','1024','--warm',str(300 if workload=='machines_sleeping'else 60),'--steps','120','--profile','1','--sleep','1','--output',str(dest)]
   with(out/(name+'.log')).open('w')as log:code=subprocess.run(cmd,cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=60).returncode
   summary=json.loads((dest/'summary.json').read_text());record['runs'].append({'name':name,'label':label,'workload':workload,'repeat':repeat,'command':cmd,'exit':code,'summary':summary});print(name,code,summary['input_and_step_ms'],flush=True)
   (out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n')
record['binaries_unchanged']=fp=={k:sha(p)for k,p in bins.items()};record['pass']=record['binaries_unchanged']and all(x['exit']==0 and x['summary']['pass']for x in record['runs']);record['utc_end']=datetime.datetime.now(datetime.timezone.utc).isoformat();(out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n')

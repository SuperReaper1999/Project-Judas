from pathlib import Path
import datetime,hashlib,json,shlex,subprocess,time
root=Path.cwd();out=root/'.cache/prealpha-pass3/ccd/final';out.mkdir(exist_ok=False)
record={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'commands':[],'source':{}}
for name in ['src/PhysicsWorld.cpp','src/Contacts.cpp','src/CollisionGeometry.cpp','src/CollisionGeometry.h','tests/ImpactMeshCostTests.cpp']:
 record['source'][name]=hashlib.sha256((root/name).read_bytes()).hexdigest()
for label,binary in [('frozen',root/'.cache/prealpha-pass3/baseline/bin/judas_impact_mesh_cost_tests'),('final',root/'build/judas_impact_mesh_cost_tests')]:
 command=[str(binary),'60','trough'];start=time.monotonic()
 with (out/(label+'-60-trough.log')).open('w') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,timeout=120)
 record['commands'].append({'label':label,'command':command,'returncode':r.returncode,'wall_seconds':time.monotonic()-start,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()})
 logs=(out/(label+'-60-trough.log')).read_text();scenarios={}
 for line in logs.splitlines():
  if line.startswith('SCENARIO '):
   row=shlex.split(line);scenarios[row[1]]={row[i]:row[i+1] for i in range(2,len(row),2)}
 record[label]={'scenarios':scenarios,'failures':[line for line in logs.splitlines() if line.startswith('FAIL ')],'summary':logs.splitlines()[-1]}
compare={}
for name,before in record['frozen']['scenarios'].items():
 after=record['final']['scenarios'][name];fields=[key for key in before if key not in {'totalMs','worstStepMs'}];compare[name]={'identical_non_timing_fields':all(before[key]==after[key] for key in fields),'digest':after['digest'],'physical_field_count':len(fields),'baseline_total_ms':float(before['totalMs']),'candidate_total_ms':float(after['totalMs']),'baseline_worst_ms':float(before['worstStepMs']),'candidate_worst_ms':float(after['worstStepMs'])}
record['comparison']=compare;record['passed']=all(v['identical_non_timing_fields'] for v in compare.values()) and record['frozen']['failures']==record['final']['failures'] and record['frozen']['summary']==record['final']['summary']=='checks 193 failures 4'
(out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({'comparison':compare,'preserved_original_failures':record['final']['failures'],'passed':record['passed']},indent=2))
raise SystemExit(0 if record['passed'] else 1)

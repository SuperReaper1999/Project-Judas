from pathlib import Path
import subprocess,time,json
r=Path.cwd();d=r/'.cache/prealpha-pass5/dense/reciprocal';records=[]
# Both timings are declared before execution. The first invalid/unpaired attempt
# remains separate and is never used as a matched improvement observation.
for name,binaries in [('matched-A',d/'baseline'),('matched-B',d/'candidate')]:
 args=['python3',str(r/'docs/evidence/prealpha-pass4/dense/reproduction/run_workloads.py'),'--root',str(r),'--live',str(binaries/'judas_physics_stress_live_benchmark'),'--scaling',str(binaries/'judas_physics_scaling_diagnostics'),'--output',str(d/name)]
 start=time.monotonic();p=subprocess.run(args,cwd=r);actual=json.loads((d/name/'RESULTS.json').read_text());children=actual['results'];passed=p.returncode==0 and len(children)==5 and all(c['exit']==0 for c in children) and children[0].get('completed_steps')==270 and all(c.get('summary',{}).get('pass')for c in children[1:]);records.append({'name':name,'argv':args,'exit':p.returncode,'seconds':time.monotonic()-start,'allFiveChildCommandsPassed':passed,'childExits':[c['exit']for c in children]});(d/'PAIRED_COMMANDS.json').write_text(json.dumps(records,indent=2)+'\n');assert passed,records[-1];print('VALIDATED',name,flush=True)

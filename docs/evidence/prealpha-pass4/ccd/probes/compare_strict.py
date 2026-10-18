from pathlib import Path
import json,re,shlex
root=Path(__file__).parent;folder=root/'strict-v6-front-fast';records={}
for label in ['baseline','candidate']:
 rows={}
 for line in (folder/(label+'.log')).read_text().splitlines():
  if not line.startswith('SCENARIO '):continue
  tokens=shlex.split(line);rows[tokens[1]]={tokens[i]:tokens[i+1] for i in range(2,len(tokens),2)}
 records[label]=rows
result={'kind':'one strict-idle sequential matched pair, same60-stepfixture and original90-stepfast-boxinputs, allownedCPUjobsheldandWindowsVMsuspended','scenarios':{}}
physical=['triangles','steps','maxPenetrationMm','atStep','meanDeepestMm','impactQueries','searchIterations','searchLimits','searchStalls','samplingFallbacks','samplingTests','impactEvents','digest']
for name,b in records['baseline'].items():
 a=records['candidate'][name];result['scenarios'][name]={'physical_fields_equal':{k:b[k]==a[k] for k in physical},'baseline':b,'candidate':a,'cost_ratio':float(a['totalMs'])/float(b['totalMs']),'worst_ratio':float(a['worstStepMs'])/float(b['worstStepMs'])}
(folder/'COMPARISON.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:{'physical_equal':all(v['physical_fields_equal'].values()),'cost_ratio':v['cost_ratio'],'worst_ratio':v['worst_ratio']} for k,v in result['scenarios'].items()},indent=2))

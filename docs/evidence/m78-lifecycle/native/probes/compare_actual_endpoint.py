#!/usr/bin/python3
"""Compare actual native paired records by durable identity, excluding fresh native slots only."""
import json,hashlib,copy
from pathlib import Path
E=Path('docs/evidence/m78-lifecycle/native')
a=E/'m78-lifecycle-mixed-endpoint-write-physical-final.json';b=E/'m78-lifecycle-mixed-endpoint-read-physical-final.json'
x=json.loads(a.read_text());y=json.loads(b.read_text())
def normalize(v):
 v=copy.deepcopy(v)
 for row in v['bodies']:
  row.pop('nativeHandle');row['completedSamples'].sort(key=lambda r:json.dumps(r,sort_keys=True))
 v['bodies'].sort(key=lambda r:r['entity'])
 v['currentPhysicalEndpoint']['actualBodies'].sort(key=lambda r:r['entity'])
 for s in v['currentPhysicalEndpoint']['supports']:s['completedContactSamples'].sort(key=lambda r:json.dumps(r,sort_keys=True))
 v['currentPhysicalEndpoint']['supports'].sort(key=lambda r:r['entity'])
 return v
p=normalize(x);q=normalize(y);assert p==q,'Actual physical records differ beyond newly allocated native slots or traversal order'
assert x['currentPhysicalEndpoint']['currentPhysicalStanding'] and y['currentPhysicalEndpoint']['currentPhysicalStanding']
report={'scope':'Independent actual physical endpoint comparison, within this native pair only. No controller-cached status substitute. No numerical tolerance added. Linux/Windows trajectories are not asserted bit-identical.','sourceFiles':[{'path':str(f),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}for f in[a,b]],'rawRecordEquality':x==y,'excludedIdentityField':'Only bodies[].nativeHandle, a fresh runtime slot identifier, not physical data','durableBodyRecords':len(x['bodies']),'mappedArticulationRecords':len(x['mapped']),'completedContactRows':sum(len(r['completedSamples'])for r in x['bodies']),'canonicalBodyAndContactOrder':'Entity identity plus deterministic JSON sorting; no rows discarded','normalizedRecordExactEquality':p==q,'actualEndpoint':{k:v for k,v in x['currentPhysicalEndpoint'].items()if k not in['actualBodies','supports']}}
(E/'ACTUAL_ENDPOINT_BY_ENTITY.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items()if k not in['sourceFiles','actualEndpoint']},indent=2))

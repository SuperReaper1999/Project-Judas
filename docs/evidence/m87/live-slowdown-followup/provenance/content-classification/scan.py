#!/usr/bin/env python3
"""Read-only M87 scene/core classification; writes only this evidence directory."""
import collections,csv,hashlib,json,subprocess
from pathlib import Path
ROOT=Path('/home/conner/Documents/GitHub/Project-Judas')
OUT=Path(__file__).resolve().parent
BASE='cee55a898aa0e4938665903f3b207d828acdb1df'
FILES=['tools/CreatePhysicsStressLab.py','projects/physics_stress_lab/Scenes/room1024.judas','projects/physics_stress_lab/Assets/scripts/lab.js']
provenance={}
for relative in FILES:
 data=(ROOT/relative).read_bytes()
 base=subprocess.check_output(['git','show',BASE+':'+relative],cwd=ROOT)
 provenance[relative]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'matches_original_commit':data==base}
scene=json.loads((ROOT/FILES[1]).read_text())['data']['objects']
bodies=[o for o in scene if 'body' in o['fields']]
bounds=[]
for o in bodies:
 f=o['fields'];p=f['position'];h=f['body.half-extents'];bounds.append(([p[k]-h[k] for k in range(3)],[p[k]+h[k] for k in range(3)]))
overlaps=[]
for i in range(261,len(bodies)):
 a,b=bounds[i]
 for j in range(len(bodies)):
  if j==i or (j>=261 and j<i):continue
  c,d=bounds[j]
  if all(min(b[k],d[k])-max(a[k],c[k])>0 for k in range(3)):overlaps.append([i,j])
core=json.load(open('/tmp/m87-live-diagnostics/original-core.json'))
pairs=core['first_25_remaining_events']
selected={str(i):{'authored_id':bodies[i]['id'],'name':bodies[i]['name'],'position':bodies[i]['fields']['position']} for i in [0,1,256,257,260,261,395,667,1152,1168,1284]}
rows=list(csv.DictReader(open('/tmp/m87-live-diagnostics/dense-fixed-before/steps.csv')))
chronology=[r for r in rows if int(r['step']) in [1,3,5,10,20,60,90,91,100,120,140,150,155]]
result={'source_commit':BASE,'source_provenance':provenance,'authored_body_count':len(bodies),'body_slot_ranges':{'floor':[0,0],'cabinets':[1,256],'pit_walls':[257,260],'loose':[261,1284]},'selected_slots':selected,'initial_positive_volume_loose_aabb_overlaps':overlaps,'remaining_25_pair_classification':dict(collections.Counter('loose-loose' if all(k>=261 for k in p[:2]) else 'other' for _,p in pairs)),'core_spinning_pairs':core['spinning_pairs'],'core_highest_inclusion_bodies':core['highest_inclusion_bodies'],'core_step_stats':core['step_stats'],'chronology':chronology}
(OUT/'source-layout-results.json').write_text(json.dumps(result,indent=2)+'\n')
(OUT/'original-core-provenance.json').write_text(Path('/tmp/m87-live-diagnostics/original-core-provenance.json').read_text())
print(json.dumps({'body_count':len(bodies),'initial_loose_overlaps':len(overlaps),'remaining_events':result['remaining_25_pair_classification'],'source_matches':all(x['matches_original_commit'] for x in provenance.values())},indent=2))

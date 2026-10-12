#!/usr/bin/env python3
import json,pathlib,hashlib
r=pathlib.Path.cwd();ev=r/'docs/evidence/m78-cold-boundary'
names={n:ev/f'm78-diagnostic-boundary-v1-{n}-joint-boundary.jsonl'for n in ['source','seed-off','restored']}
all={n:[json.loads(x)for x in p.read_text().splitlines()]for n,p in names.items()};all={n:[x for x in data if x['tick']==0 and x['prepare']==1]for n,data in all.items()}
free=lambda es:{x['body']:x['state']for x in es if x['kind']=='prepare_free_body'}
row=lambda es:{(x['jointOrdinal'],x['cacheIndex']):x for x in es if x['kind']=='row_construct'}
joint=lambda es:{x['jointOrdinal']:x for x in es if x['kind']=='joint_begin'}
baseFree=free(all['source']);baseRow=row(all['source']);baseJoint=joint(all['source']);out={}
for name in ['seed-off','restored']:
 otherJ=joint(all[name]);mapping={}
 for k,j in baseJoint.items():
  for s in ['A','B']:
   old=j['body'+s];new=otherJ[k]['body'+s]
   if old in mapping:assert mapping[old]==new
   mapping[old]=new
 otherFree=free(all[name]);otherRows=row(all[name]);physicalDiff=[];equationDiff=[];commandsDiff=[];firstWarm=[]
 for id,x in baseFree.items():
  y=otherFree[mapping[id]]
  for k,v in x.items():
   if v!=y[k]:physicalDiff.append({'bodySource':id,'bodyOther':mapping[id],'field':k,'source':v,'other':y[k]})
 keys=['linearA','linearB','angularA','angularB','effective','mass','error','errorVector','bias','gamma','low','high']
 for key,x in baseRow.items():
  if key not in otherRows:continue
  y=otherRows[key]
  for k in keys:
   if x.get(k)!=y.get(k):equationDiff.append({'row':key,'field':k,'source':x.get(k),'other':y.get(k)})
  if x['initialImpulse']!=y['initialImpulse']:firstWarm.append({'row':key,'source':x['initialImpulse'],'other':y['initialImpulse']})
 for k,x in baseJoint.items():
  y=otherJ[k]
  for field,v in x['drive'].items():
   if field!='impulses'and v!=y['drive'][field]:commandsDiff.append({'joint':k,'field':field,'source':v,'other':y['drive'][field]})
 out[name]={'bodyHandleRemap':mapping,'physicalPrePrepareDifferences':physicalDiff,'rowEquationDifferencesCommonRows':equationDiff,'authoritativeCommandDifferences':commandsDiff,'missingRows':sorted(set(baseRow)-set(otherRows)),'initialWarmImpulseDifferences':firstWarm,'sourceRows':len(baseRow),'otherRows':len(otherRows),'firstLimitPrediction':[x for x in all[name]if x['kind']=='limit_prediction']}
out['scope']='Exact native a106 saved snapshot on same compiled diagnostic; compare all physical pre-Prepare states, actual drive commands, row equations and initial guesses. All three snapshot hashes independently verified. No cross-platform-input equivalence claim.';out['journals']={n:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}for n,p in names.items()};(ev/'native-first-boundary-comparison.json').write_text(json.dumps(out,indent=2,allow_nan=False)+'\n')
for n in ['seed-off','restored']:
 d=out[n];print(n,{k:len(d[k])for k in ['physicalPrePrepareDifferences','rowEquationDifferencesCommonRows','authoritativeCommandDifferences','missingRows','initialWarmImpulseDifferences']})

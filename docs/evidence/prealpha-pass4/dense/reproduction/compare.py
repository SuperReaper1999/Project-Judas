#!/usr/bin/env python3
from pathlib import Path
import csv,json,argparse,hashlib,datetime
p=argparse.ArgumentParser();p.add_argument('baseline',type=Path);p.add_argument('candidate',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
fields=['step','bodies','awake','sleeping','candidates','contacts','impact_events','impact_queries','impact_iterations','impact_peak','trajectory_digest','max_linear_speed','max_angular_speed','nonfinite_fields']
left=list(csv.DictReader((a.baseline/'dense270/steps.csv').open()));right=list(csv.DictReader((a.candidate/'dense270/steps.csv').open()));diff=[]
for n,(l,r)in enumerate(zip(left,right)):
 for key in fields:
  if l[key]!=r[key]:diff.append({'step':n+1,'field':key,'baseline':l[key],'candidate':r[key]})
lquality=json.loads((a.baseline/'dense270/quality.json').read_text());rquality=json.loads((a.candidate/'dense270/quality.json').read_text());qd=[key for key in lquality if key!='observation_ms'and lquality[key]!=rquality[key]]
controls={}
for name in ['free1024','pile256','machines_active256','machines_sleeping256']:
 l=json.loads((a.baseline/name/'summary.json').read_text());r=json.loads((a.candidate/name/'summary.json').read_text());controls[name]={'baseline_pass':l.get('pass'),'candidate_pass':r.get('pass'),'digest_equal':l.get('trajectory_digest')==r.get('trajectory_digest'),'baseline':l,'candidate':r}
lresults=json.loads((a.baseline/'RESULTS.json').read_text());rresults=json.loads((a.candidate/'RESULTS.json').read_text())
out={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'baseline':str(a.baseline),'candidate':str(a.candidate),'same_step_count':len(left)==len(right),'all_physical_fields_equal':not diff and len(left)==len(right),'steps':len(right),'different_fields':diff,'endpoint_quality_equal':not qd,'endpoint_quality_differences':qd,'physical_fields':fields,'controls':controls,'baseline_timing':lresults['results'][0],'candidate_timing':rresults['results'][0]}
a.output.write_text(json.dumps(out,indent=2)+'\n');print(json.dumps({k:v for k,v in out.items()if k not in ['controls','baseline_timing','candidate_timing','different_fields']},indent=2));print('differences',len(diff));print('timing',lresults['results'][0].get('fixed_ms'),'->',rresults['results'][0].get('fixed_ms'))

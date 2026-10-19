#!/usr/bin/env python3
"""Reuse exact Bernstein proof over full requested start-to-finish intervals."""
from pathlib import Path
import hashlib,json
OWNED=Path(__file__).resolve().parent
source=(OWNED/'interval-separation-proof.py').read_text()
exec(compile(source.split('\ntraces=')[0],str(OWNED/'interval-separation-proof.py'),'exec'))

traces=[json.loads(x) for x in (OWNED/'pair-trace.jsonl').read_text().splitlines()]
endpoint=json.loads((OWNED/'smooth-gap-proof.json').read_text())
results=[]
for item in endpoint['queries']:
    event=item['selected_event']
    if not event:continue
    trace=traces[item['record']-1];a,b=trace['body_a'],trace['body_b']
    t0=F(f64(trace['start']));t1=F(f64(trace['finish']))
    pa,qa=smooth(a,t0);pb,qb=smooth(b,t0)
    ha,hb=vf(a['shape']['half_extents']),vf(b['shape']['half_extents'])
    start=sat(pa,qa,pb,qb,ha,hb)
    pa,qa=smooth(a,t1);pb,qb=smooth(b,t1)
    end=sat(pa,qa,pb,qb,ha,hb)
    if start['separated'] and end['separated']:
        proof=certify(a,b,t0,t1)
    else:
        proof={'certified':False,'reason':'smooth start or endpoint overlap','depth':0}
    row={'record':item['record'],'event':event['chronological_event_in_step'],
         'start_time_s':float(t0),'finish_time_s':float(t1),'start_sat':start,'finish_sat':end,
         'leaf_count':leaf_count(proof),'maximum_subdivision_depth':max_depth(proof),'proof':proof}
    results.append(row)
    print(item['record'],event['chronological_event_in_step'],proof['certified'],
          'start/end gaps',start['maximum_normalized_gap_m'],end['maximum_normalized_gap_m'],
          'leaves',leaf_count(proof),'depth',max_depth(proof),flush=True)
result={'schema':'judas.exact-rational-obb-full-query-interval.v1',
        'method':__doc__,'input_sha256':{n:hashlib.sha256((OWNED/n).read_bytes()).hexdigest() for n in ['interval-separation-proof.py','pair-trace.jsonl','smooth-gap-proof.json']},
        'results':results}
(OWNED/'full-interval-separation-proof.json').write_text(json.dumps(result,indent=2)+'\n')

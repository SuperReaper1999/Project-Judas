#!/usr/bin/env python3
"""Diagnostic smooth arrival brackets plus certified separated prefixes.

Bisection alone is not a first-arrival proof for rotating shapes. Accordingly,
this report certifies a strict separated prefix independently and only reports
a pointwise overlap bracket thereafter. No production numerical policy changes.
"""
from pathlib import Path
import json,hashlib
OWNED=Path(__file__).resolve().parent
exec(compile((OWNED/'interval-separation-proof.py').read_text().split('\ntraces=')[0],str(OWNED/'interval-separation-proof.py'),'exec'))
traces=[json.loads(x) for x in (OWNED/'pair-trace.jsonl').read_text().splitlines()]
endpoint=json.loads((OWNED/'smooth-gap-proof.json').read_text())
rows=[]
for item in endpoint['queries']:
    event=item['selected_event']
    if not event or not item['probes'][0]['models']['precise_cayley_double_elapsed']['separated']:continue
    trace=traces[item['record']-1];a,b=trace['body_a'],trace['body_b']
    t0=F(f64(trace['start']));finish=F(f64(trace['finish']));native=F(f64(trace['result_time']))
    ha,hb=vf(a['shape']['half_extents']),vf(b['shape']['half_extents'])
    def separation(t):
        pa,qa=smooth(a,t);pb,qb=smooth(b,t)
        return sat(pa,qa,pb,qb,ha,hb)
    lo,hi=t0,finish
    for i in range(28):
        mid=(lo+hi)/2
        if separation(mid)['separated']:lo=mid
        else:hi=mid
    # A bounded exact first-arrival exclusion with meaningful distance from root.
    prefix=t0+F(9,10)*(lo-t0)
    proof=certify(a,b,t0,prefix)
    rows.append({'record':item['record'],'event':event['chronological_event_in_step'],
      'start_s':float(t0),'native_false_arrival_s':float(native),
      'pointwise_smooth_separated_time_s':float(lo),'pointwise_smooth_overlapping_time_s':float(hi),
      'certified_separated_prefix_end_s':float(prefix),'prefix_certified':proof['certified'],
      'prefix_leaf_count':leaf_count(proof),'prefix_max_depth':max_depth(proof),
      'prefix_proof':proof,'separated_probe':separation(lo),'overlapping_probe':separation(hi)})
    print(item['record'],event['chronological_event_in_step'],'native',float(native),'smooth bracket',float(lo),float(hi),'cert prefix',float(prefix),proof['certified'],leaf_count(proof),max_depth(proof),flush=True)
(OWNED/'smooth-arrival-brackets.json').write_text(json.dumps({'schema':'judas.smooth-arrival-brackets.v1','method':__doc__,
    'inputs_sha256':{n:hashlib.sha256((OWNED/n).read_bytes()).hexdigest() for n in ['pair-trace.jsonl','smooth-gap-proof.json','interval-separation-proof.py']},
    'results':rows},indent=2)+'\n')

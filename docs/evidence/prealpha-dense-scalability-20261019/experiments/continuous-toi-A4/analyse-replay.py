#!/usr/bin/env python3
"""Independent exact-rational checks of research TOI retiming outputs.

This is not a baseline-time parity test: corrected arrival times are expected
to change. Physical acceptance still requires a real native manifold and no
strict separation under the continuous source trajectory. Dropped queries
with smooth endpoint overlap are flagged explicitly, never accepted as no-hit.
"""
from pathlib import Path
import json,hashlib,struct
OWNED=Path(__file__).resolve().parent
ORIGINAL=OWNED.parent/'event-topology-observer-v2'
ns={'__file__':str(ORIGINAL/'smooth-gap-proof.py')}
exec(compile((ORIGINAL/'smooth-gap-proof.py').read_text().split('\ntraces=')[0],str(ORIGINAL/'smooth-gap-proof.py'),'exec'),ns)
F,f64,vf,smooth,sat=[ns[k] for k in ['F','f64','vf','smooth','sat']]
trace=[json.loads(x) for x in (ORIGINAL/'pair-trace.jsonl').read_text().splitlines()]
old=json.loads((ORIGINAL/'pair-replay.json').read_text())
new=json.loads((OWNED/'pair-replay.json').read_text())
old_by_record={q['record']:q for q in old['queries']}
rows=[]
for q in new['queries']:
    captured=trace[q['record']-1];a,b=captured['body_a'],captured['body_b']
    before=old_by_record[q['record']]['native_replay_result'];now=q['native_replay_result']
    finish=q['finish'];ha,hb=vf(a['shape']['half_extents']),vf(b['shape']['half_extents'])
    def exact_at(t):
        pa,qa=smooth(a,F(t));pb,qb=smooth(b,F(t));return sat(pa,qa,pb,qb,ha,hb)
    result={'record':q['record'],'original_native_time_s':before,'candidate_time_s':now,
            'time_change_s':now-before,'candidate_hit_within_interval':now<=finish,
            'start_continuous_sat':exact_at(q['start']),
            'finish_continuous_sat':exact_at(finish)}
    if now<=finish:
        exact=exact_at(now)
        native=next(p for p in q['probes'] if p['time']==now)
        result.update({'candidate_continuous_sat':exact,'candidate_native_manifold_rows':native['actual_rows'],
                        'false_arrival_remaining':exact['separated'],
                        'missing_actual_manifold':native['actual_rows']==0})
    else:
        result['dropped_despite_smooth_endpoint_overlap']=not result['finish_continuous_sat']['separated']
    rows.append(result)
summary={'queries':len(rows),'times_changed':sum(r['time_change_s']!=0 for r in rows),
         'false_arrivals_remaining':sum(r.get('false_arrival_remaining',False) for r in rows),
         'missing_actual_manifolds':sum(r.get('missing_actual_manifold',False) for r in rows),
         'dropped_despite_smooth_endpoint_overlap':sum(r.get('dropped_despite_smooth_endpoint_overlap',False) for r in rows)}
result={'schema':'judas.retimed-toi-independent-diagnostic.v1','method':__doc__,
        'inputs_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ORIGINAL/'pair-trace.jsonl',ORIGINAL/'pair-replay.json',OWNED/'pair-replay.json',ORIGINAL/'smooth-gap-proof.py']},
        'summary':summary,'queries':rows}
(OWNED/'REPLAY_ANALYSIS.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(summary,indent=2))
for r in rows:print(r['record'],r['time_change_s'],r.get('false_arrival_remaining'),r.get('dropped_despite_smooth_endpoint_overlap'))

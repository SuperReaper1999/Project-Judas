#!/usr/bin/env python3
"""Preserve rejected experiment and derive a bounded, reproducible summary."""
from pathlib import Path
import csv,hashlib,json,math,shutil
SRC=Path('/tmp/m87-support-continuation')
OUT=Path('/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m87/live-slowdown-followup/experiments/support-continuation-rejected')
OUT.mkdir(parents=True,exist_ok=True)
for name in ['PhysicsWorld.base.cpp','PhysicsWorld.cpp','support-continuation.diff','README.md','build.py','prepare.py','build.log','provenance.json','collate.py']:
 shutil.copy2(SRC/name,OUT/name)
for mode in ['control-155','projection-155']:
 shutil.copy2(SRC/(mode+'.log'),OUT/(mode+'.log'))
 (OUT/mode).mkdir(exist_ok=True)
 for child in (SRC/mode).iterdir():
  if child.is_file():shutil.copy2(child,OUT/mode/child.name)
def rows(mode):
 with(SRC/mode/'steps.csv').open()as f:return list(csv.DictReader(f))
def percentile(values,q):
 s=sorted(values);x=(len(s)-1)*q;lo=math.floor(x);hi=math.ceil(x);return s[lo]+(s[hi]-s[lo])*(x-lo)
def summarize(data):
 times=[float(x['fixed_ms'])for x in data]
 return {'steps':len(data),'fixed_ms_sum':sum(times),'fixed_ms_median':percentile(times,.5),'fixed_ms_p95_linear':percentile(times,.95),'fixed_ms_max':max(times),
         'worst_step':int(max(data,key=lambda x:float(x['fixed_ms']))['step']),
         'impact_events_sum':sum(int(x['impact_events'])for x in data),'impact_queries_sum':sum(int(x['impact_queries'])for x in data),
         'nonfinite_fields_sum':sum(int(x['nonfinite_fields'])for x in data),'max_linear_speed':max(float(x['max_linear_speed'])for x in data),'max_angular_speed':max(float(x['max_angular_speed'])for x in data)}
def trace(mode):
 records=[]
 for line in(SRC/(mode+'.log')).read_text().splitlines():
  if line.startswith('CONTINUATION '):records.append({k:float(v)for k,v in (token.split('=',1)for token in line.split()[1:])})
 sums=['attempts','applied','rows','positive','added','rollback','unsupported']
 maxima=['energy_rise','momentum_residual','angular_residual','velocity_violation']
 return {'records':len(records),'totals':{k:sum(x[k]for x in records)for k in sums},'maxima':{k:max((x[k]for x in records),default=0)for k in maxima}}
a=rows('control-155');b=rows('projection-155');assert len(a)==len(b)==155
first=next(((x,y)for x,y in zip(a,b)if x['trajectory_digest']!=y['trajectory_digest']),None)
assert all(x['step']==y['step']for x,y in zip(a,b))
summary={'classification':'REJECTED isolated numerical-policy experiment; not production or physical-quality acceptance',
 'same_binary':str(SRC/'live'),'binary_sha256':hashlib.sha256((SRC/'live').read_bytes()).hexdigest(),
 'controls':{'input_scene':'/tmp/judas-m87-watch-ecpbqnfz/Scenes/room1024.judas','frame_seconds_argument':'0.016666667','completed_fixed_steps':155,'tour':False,'hidden':True,'swap':0,'profiler':False,'trace':True,'exit_codes':{'control':0,'projection':0}},
 'control':summarize(a),'projection':summarize(b),'control_trace':trace('control-155'),'projection_trace':trace('projection-155'),
 'first_physical_divergence':None if first is None else {'step':int(first[0]['step']),'control':first[0],'projection':first[1]},
 'matching_physical_step_digests':sum(x['trajectory_digest']==y['trajectory_digest']for x,y in zip(a,b)),
 'limits':['Physical trajectory changes by design; no exact-behaviour claim.','The extra normal projection uses 10 additional iterations per accepted event.','No lowered CCD policy, changed ordinary invalidation/rescheduling, gap exemption or event cap.','Finite fields and local diagnostic budgets do not establish full physical correctness.','Residual normal violations remain; ordinary CCD was deliberately retained.','One cosmetic prototype-only compiler warning; this experiment was rejected rather than polished or tuned.','All timings include diagnostic trace and are a same-binary rejection comparison, not final production benchmark numbers.']}
(OUT/'SUMMARY.json').write_text(json.dumps(summary,indent=2)+'\n')
commands='''#!/usr/bin/env bash
# Exact original parent invocations; both completed with exit code 0.
env JUDAS_ENGINE_ROOT=/home/conner/Documents/GitHub/Project-Judas JUDAS_SUPPORT_CONTINUATION_ORIGINAL=1 JUDAS_SUPPORT_CONTINUATION_TRACE=1 timeout --signal=TERM --kill-after=3s 45s /tmp/m87-support-continuation/live /tmp/judas-m87-watch-ecpbqnfz/Scenes/room1024.judas /tmp/m87-support-continuation/control-155 100 155 0 0 1 0.016666667 0 0 > /tmp/m87-support-continuation/control-155.log 2>&1
env JUDAS_ENGINE_ROOT=/home/conner/Documents/GitHub/Project-Judas JUDAS_SUPPORT_CONTINUATION_TRACE=1 timeout --signal=TERM --kill-after=3s 45s /tmp/m87-support-continuation/live /tmp/judas-m87-watch-ecpbqnfz/Scenes/room1024.judas /tmp/m87-support-continuation/projection-155 100 155 0 0 1 0.016666667 0 0 > /tmp/m87-support-continuation/projection-155.log 2>&1
'''
(OUT/'commands.sh').write_text(commands)
x=summary['control'];y=summary['projection'];t=summary['projection_trace'];f=summary['first_physical_divergence']
report=f'''# Rejected: event-time persistent-support projection

## Decision

**Reject; not integrated.** The proposed continuation mechanism changes physical trajectories and adds an extra normal solve. It did not improve the original dense fixture. No further tuning or acceptance testing is justified by this result.

The complete source, base source, patch, preparation/build scripts, build log and provenance are preserved here. Raw fixed-step/frame CSVs and diagnostic logs preserve both exact same-binary controls. The executable/object remain outside the repository; the binary hash is in `SUMMARY.json`. No binaries, objects or generated build directories are included.

## Identical input and settings

Both runs used the original current 1024-loose-body scene, 1285 total bodies, 1152 dynamic bodies, original materials/shapes, fixed input cadence argument `0.016666667`, 155 completed fixed steps, no tour, hidden rendering, no swap wait and no profiler recording. Both used trace logging and exited zero. See `commands.sh` for exact original invocations. Only the private experiment enable flag differed.

| Measured fixed-step result | Original control | Continuation projection |
|---|---:|---:|
| Sum over 155 steps (ms) | {x['fixed_ms_sum']:.6f} | {y['fixed_ms_sum']:.6f} |
| Median (ms) | {x['fixed_ms_median']:.6f} | {y['fixed_ms_median']:.6f} |
| p95 linear percentile (ms) | {x['fixed_ms_p95_linear']:.6f} | {y['fixed_ms_p95_linear']:.6f} |
| Maximum (ms) | {x['fixed_ms_max']:.6f} | {y['fixed_ms_max']:.6f} |
| Worst fixed step | {x['worst_step']} | {y['worst_step']} |
| Impact events | {x['impact_events_sum']} | {y['impact_events_sum']} |
| Impact queries | {x['impact_queries_sum']} | {y['impact_queries_sum']} |
| Nonfinite physical fields | {x['nonfinite_fields_sum']} | {y['nonfinite_fields_sum']} |

These are diagnostic rejection timings, including identical trace logging, and must not be substituted for the final production before/after measurements.

## Physical divergence and local diagnostics

The first physical digest divergence occurs at step **{f['step']}**. {summary['matching_physical_step_digests']} of 155 step digests match. Full first-divergence input/result records are preserved in `SUMMARY.json`; changed trajectories were expected because this is a policy change.

Across the projection run:

- Attempts: {int(t['totals']['attempts'])}; accepted projections: {int(t['totals']['applied'])}.
- Extra solved rows: {int(t['totals']['rows'])}; positive-gap rows: {int(t['totals']['positive'])}.
- Additional component-member inclusions: {int(t['totals']['added'])} (repeated inclusions across events, not unique bodies).
- Energy-guard rollbacks: {int(t['totals']['rollback'])}; unsupported-case skips: {int(t['totals']['unsupported'])}.
- Largest measured positive kinetic-energy rise: {t['maxima']['energy_rise']:.12g} J (within the existing relative engineering guard for that component).
- Largest momentum residual after fixed-boundary reactions: {t['maxima']['momentum_residual']:.12g} kg m/s.
- Largest angular-momentum residual after fixed-boundary reactions: {t['maxima']['angular_residual']:.12g} kg m^2/s.
- Largest remaining normal-velocity constraint violation: {t['maxima']['velocity_violation']:.12g} m/s.

These are narrow local diagnostics, **not** physical-quality acceptance. Finite fields or a passing energy guard do not establish correct complete mechanical behaviour, penetration bounds, game behaviour or persistence. The nonzero normal residual is retained explicitly. Ordinary CCD remained active as the nonlinear/finite-iteration safeguard.

## What was changed experimentally

The true zero-margin `ImpactSolver` remains unchanged. A separate, zero-warm, normal-only support projection refreshes previously accepted support anchors at event time, with target `-max(gap,0)/(dt-now)`. It includes existing hard-joint impact rows while excluding additional motor/spring/constitutive efforts. Uncertain geometry, incompatible anchors, newly reached sleeping/deferred-acceleration bodies and moving infinite-mass boundaries retain the original path. The complete original CCD invalidation/rescheduling, support exemption policy and event-cap behaviour remain unchanged.

This adds **10 extra iterations per accepted projection**, and applies predictive normal response to existing positive-gap supports, just as the existing persistent-support concept permits. It is not an exact optimization and is not a universal no-impulse-across-gap model. Positive-gap rows do not publish collision callbacks. No physical cadence, contact fidelity, ordinary CCD search, fixture threshold or damping value was reduced.

Compilation/linking succeeded with one cosmetic prototype-only misleading-indentation warning preserved in `build.log`. Since the measured mechanism was rejected, it was not polished, retuned or merged. The validated production cost reductions and remaining dense-pile limitation are separate from this experiment.
'''
(OUT/'RESULT.md').write_text(report)
manifest=[]
for p in sorted(OUT.rglob('*')):
 if p.is_file()and p.name!='SHA256.json':manifest.append({'path':str(p.relative_to(OUT)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(OUT/'SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'output':str(OUT),'files':len(manifest)+1,'control':x,'projection':y,'first_divergence':f['step'],'projection_trace':t},indent=2))

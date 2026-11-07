#!/usr/bin/env python3
"""Exercise real MK2/grid-water scenes and cold processes, preserving raw results."""
import argparse
import csv
import datetime
import hashlib
import json
import math
from pathlib import Path
import statistics
import subprocess
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--executable',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--steps',type=int,default=1200)
a=p.parse_args()
if a.steps<1200: p.error('Full qualification needs at least 1200 steps for the vent controls.')
exe=a.executable.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
results={};checks=[]
def check(name,ok,detail): checks.append({'name':name,'passed':bool(ok),'detail':detail})
def run(name,args,steps):
    csv_path=out/(name+'.csv');started=datetime.datetime.now(datetime.timezone.utc).isoformat();start=time.monotonic()
    command=[str(exe),'--headless','--steps',str(steps),'--log',str(csv_path),*args]
    with (out/(name+'.log')).open('w') as log:
        proc=subprocess.run(['/usr/bin/time','-v','-o',str(out/(name+'.resource')),*command],stdout=log,stderr=subprocess.STDOUT,timeout=180)
    rows=list(csv.DictReader(csv_path.open())) if csv_path.exists() else []
    result={'command':command,'started_utc':started,'elapsed_seconds':time.monotonic()-start,'exit':proc.returncode,'rows':len(rows)}
    if rows:
        for key in ('fluid_ms','scene_ms'):
            values=sorted(float(row[key]) for row in rows)
            result[key]={'median':statistics.median(values),'p95':values[math.ceil(.95*len(values))-1],'worst':max(values)}
        first=float(rows[0]['water']);result['water']=first
        result['maximum_water_drift']=max(abs(float(r['water'])-first) for r in rows)
        result['maximum_stored_displacement']=max(float(r['stored']) for r in rows)
        result['maximum_body_speed']=max(float(r['float_speed']) for r in rows)
    results[name]=result
    check(name+'.completion',proc.returncode==0 and len(rows)==steps,result)
    if len(rows)==steps:
        check(name+'.finite',all(math.isfinite(float(v)) for r in rows for v in r.values()),'All recorded physical and timing values are finite.')
        check(name+'.quantity',result['maximum_water_drift']<=1e-6*max(result['water'],1),result['maximum_water_drift'])
        if 'air_accounted' in rows[0]:
            initial_air=float(rows[0]['air_accounted']);drift=max(abs(float(r['air_accounted'])-initial_air) for r in rows)
            check(name+'.air-ledger',drift<1e-6*max(initial_air,1),{'initial_kg':initial_air,'max_drift_kg':drift})
        check(name+'.no-launch',result['maximum_body_speed']<10,result['maximum_body_speed'])
        check(name+'.bounded-displacement',result['maximum_stored_displacement']<=.02*result['water'],result['maximum_stored_displacement'])
    print(name,'exit',proc.returncode,'rows',len(rows),'scene ms',result.get('scene_ms'),flush=True)
    return rows

cases=[('quiet',['--mode','pool','--no-bodies']),('pool',['--mode','pool']),
       ('flow',['--mode','flow']),('air',['--mode','air','--no-bodies']),
       ('terrain',['--mode','terrain']),('original',['--mode','original']),
       ('original-rotated',['--mode','original','--rotated'])]
data={name:run(name,args,a.steps) for name,args in cases}
if len(data['quiet'])==a.steps:
    rms=statistics.median(float(r['rms_speed']) for r in data['quiet'][-300:])
    check('quiet.settling',rms<.02,{'tail_median_water_rms_mps':rms})
if len(data['pool'])==a.steps:
    height=statistics.mean(float(r['float_y']) for r in data['pool'][-300:]);sink=statistics.mean(float(r['sink_y']) for r in data['pool'][-300:])
    check('pool.float-height',abs(height-2)<.3,{'mean_height':height,'expected_pool_level':2,'tolerance':.3})
    check('pool.dense-sinks',abs(sink-.3)<.08,{'mean_height':sink,'floor_plus_half_extent':.3})
if len(data['flow'])==a.steps:
    start=float(data['flow'][58]['left_water']);end=statistics.mean(float(r['left_water']) for r in data['flow'][-120:])
    check('flow.real-transfer',end<.8*start,{'left_before_gate':start,'left_final':end})
if len(data['air'])==a.steps:
    r=data['air'];closed=float(r[598]['primary_air_pressure']);just_open=float(r[600]['primary_air_pressure']);end=statistics.mean(float(v['primary_air_pressure']) for v in r[-120:])
    check('air.compression',closed>500,{'closed_pressure_pa':closed})
    check('air.finite-vent',just_open>.1*closed,{'closed_pa':closed,'first_open_pa':just_open})
    check('air.pressure-releases',abs(end)<.5*closed,{'closed_pa':closed,'tail_mean_pa':end})
    maximum_tail=max(abs(float(v['max_air_pressure'])) for v in r[-120:])
    check('air.no-stranded-reservoir',maximum_tail<.5*closed,
          {'closed_pa':closed,'largest_pressure_in_any_tail_reservoir_pa':maximum_tail})

save=out/'cold-save'
old=run('cold-write',['--mode','flow','--save-dir',str(save),'--save-at','180'],180)
new=run('cold-read',['--load',str(save)],180)
if old and new:
    check('cold.true-process-continuation',int(old[-1]['step'])==180 and int(new[0]['step'])==181,{'last_old_step':old[-1]['step'],'first_new_step':new[0]['step']})
    check('cold.quantity',abs(float(new[-1]['water'])-float(old[-1]['water']))<1e-6*float(old[-1]['water']),{'before':old[-1]['water'],'after':new[-1]['water']})

record={'schema':1,'source_only_preview':True,'alpha_lock_changed':False,'executable':str(exe),
        'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'results':results,'checks':checks,
        'passed':all(c['passed'] for c in checks),
        'scope':'Fixed-resolution native water + real MK2, Linux headless. Timing is CPU-only; not GPU or published-platform qualification.'}
(out/'verification.json').write_text(json.dumps(record,indent=2)+'\n')
failed=[c for c in checks if not c['passed']]
print(len(checks),'checks,',len(failed),'failures',flush=True)
for c in failed: print('FAIL',c['name'],c['detail'],flush=True)
raise SystemExit(bool(failed))

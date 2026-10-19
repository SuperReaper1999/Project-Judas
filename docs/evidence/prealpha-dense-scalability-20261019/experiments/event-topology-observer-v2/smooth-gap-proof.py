#!/usr/bin/env python3
"""Exact-rational 15-axis OBB controls for the recorded binary inputs.

The Cayley quaternion need not be normalized to construct R(q)/dot(q,q).
Thus smooth trajectories, their rotation matrices, all SAT axes and SAT signs
can be evaluated as exact fractions. No epsilon classifies these contacts.
"""
from fractions import Fraction as F
from pathlib import Path
import hashlib
import json
import math
import struct

OWNED=Path(__file__).resolve().parent
def f32(bits):return struct.unpack('<f',struct.pack('<I',bits))[0]
def f64(bits):return struct.unpack('<d',struct.pack('<Q',bits))[0]
def vf(bits):return [F(f32(v)) for v in bits]
def add(a,b):return [x+y for x,y in zip(a,b)]
def sub(a,b):return [x-y for x,y in zip(a,b)]
def mul(a,s):return [x*s for x in a]
def dot(a,b):return sum((x*y for x,y in zip(a,b)),F(0))
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def rotation(q):
    w,x,y,z=q;n=w*w+x*x+y*y+z*z
    return [[v/n for v in column] for column in [
        [w*w+x*x-y*y-z*z,2*(x*y+w*z),2*(x*z-w*y)],
        [2*(x*y-w*z),w*w-x*x+y*y-z*z,2*(y*z+w*x)],
        [2*(x*z+w*y),2*(y*z-w*x),w*w-x*x-y*y+z*z]]]
def segment(body,time):
    matching=[s for s in body['motion'] if F(f64(s['begin']))<=time<=F(f64(s['end']))]
    if not matching:raise ValueError('query outside exact recorded motion')
    return matching[-1]
def smooth(body,time,float_elapsed=False):
    s=segment(body,time)
    elapsed=time-F(f64(s['begin']))
    if float_elapsed:
        elapsed=F(struct.unpack('<f',struct.pack('<f',float(elapsed)))[0])
    p=add(vf(s['position']),mul(vf(s['linear_velocity']),elapsed))
    w,x,y,z=vf(s['orientation_wxyz']);ox,oy,oz=vf(s['drift_angular_velocity'])
    derivative=[-ox*x-oy*y-oz*z,ox*w+oy*z-oz*y,-ox*z+oy*w+oz*x,ox*y-oy*x+oz*w]
    q=add([w,x,y,z],mul(derivative,elapsed/2))
    return p,q
def sat(pa,qa,pb,qb,ha,hb):
    ra,rb=rotation(qa),rotation(qb);delta=sub(pb,pa)
    axes=ra+rb+[cross(a,b) for a in ra for b in rb]
    signs=[];winner=None
    for index,axis in enumerate(axes):
        norm2=dot(axis,axis)
        if norm2==0:signs.append(None);continue
        gap=abs(dot(delta,axis))-sum(ha[k]*abs(dot(ra[k],axis)) for k in range(3))-sum(hb[k]*abs(dot(rb[k],axis)) for k in range(3))
        signs.append(1 if gap>0 else -1 if gap<0 else 0)
        scaled=float(gap)/math.sqrt(float(norm2))
        if winner is None or scaled>winner[0]:winner=(scaled,index,gap,norm2)
    return {'separated':any(s==1 for s in signs),'all_axis_exact_signs':signs,
        'maximum_normalized_gap_m':winner[0],'winning_axis':winner[1],
        'winning_unnormalized_gap_numerator':str(winner[2].numerator),
        'winning_unnormalized_gap_denominator':str(winner[2].denominator)}

traces=[json.loads(x) for x in (OWNED/'pair-trace.jsonl').read_text().splitlines()]
replay=json.loads((OWNED/'pair-replay.json').read_text())
events=[json.loads(x) for x in (OWNED/'events.jsonl').read_text().splitlines()]
output=[]
for query in replay['queries']:
    captured=traces[query['record']-1]
    a,b=captured['body_a'],captured['body_b'];ha=vf(a['shape']['half_extents']);hb=vf(b['shape']['half_extents'])
    if any(vf(a['shape']['pivot_offset'])) or any(vf(b['shape']['pivot_offset'])):
        raise ValueError('this proof supports zero-pivot primitive boxes only')
    picked=[e for e in events if e['step']==captured['step'] and e['selected']==captured['key'] and e['event_time_s']==query['expected_result']]
    qout={'record':query['record'],'after_global_impact_sequence':captured['after_global_impact_sequence'],
          'selected_event':picked[0] if picked else None,'probes':[]}
    for p in query['probes']:
        time=F(f64(p['time_bits']))
        pa,qa=vf(p['a']['position_bits']),vf(p['a']['orientation_bits_wxyz'])
        pb,qb=vf(p['b']['position_bits']),vf(p['b']['orientation_bits_wxyz'])
        sa,sqa=smooth(a,time);sb,sqb=smooth(b,time)
        sfa,sfqa=smooth(a,time,True);sfb,sfqb=smooth(b,time,True)
        models={
            'native_represented_pose':sat(pa,qa,pb,qb,ha,hb),
            'precise_centres_native_orientation':sat(sa,qa,sb,qb,ha,hb),
            'native_centres_precise_orientation':sat(pa,sqa,pb,sqb,ha,hb),
            'precise_cayley_double_elapsed':sat(sa,sqa,sb,sqb,ha,hb),
            'precise_cayley_float_elapsed':sat(sfa,sfqa,sfb,sfqb,ha,hb)}
        if models['native_represented_pose']['separated']==p['touching_boolean']:
            raise AssertionError('independent exact SAT disagrees with native contact truth')
        qout['probes'].append({'time':p['time'],'time_bits':p['time_bits'],
            'native_rows':p['actual_rows'],'models':models})
    output.append(qout)
result={'schema':'judas.exact-rational-smooth-obb.v1',
    'method':'Exact Fraction arithmetic from captured IEEE bits. R(q)=R_raw(q)/dot(q,q); all 15 SAT signs are rational, no geometric epsilon or approximate sign.',
    'limits':'This isolates current per-segment Cayley arithmetic. It does not yet reconstruct unrounded anchors across earlier physical velocity changes or propose a production representation.',
    'input_sha256':{name:hashlib.sha256((OWNED/name).read_bytes()).hexdigest() for name in ['pair-trace.jsonl','pair-replay.json','events.jsonl']},
    'queries':output}
(OWNED/'smooth-gap-proof.json').write_text(json.dumps(result,indent=2)+'\n')
for q in output:
    if not q['selected_event']:continue
    e=q['selected_event'];p=q['probes'][-1]
    print(q['record'],e['chronological_event_in_step'],'closing',e['selected_max_closing_before_mps'])
    for name,model in p['models'].items():print(' ',name,model['maximum_normalized_gap_m'],model['separated'])

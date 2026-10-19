#!/usr/bin/env python3
"""Prove an actual smooth OBB trajectory remains separated for an entire CCD query.

All coefficients are exact rational values from recorded IEEE inputs. Multiplying
fixed-plane corner gaps by the two strictly positive quaternion squared norms
leaves degree <=5 polynomials. Positive Bernstein coefficients prove positive
separation over the whole closed interval, without a geometric tolerance.
"""
from fractions import Fraction as F
from itertools import product
from math import comb
from pathlib import Path
import hashlib,json

OWNED=Path(__file__).resolve().parent
# Reuse the mathematical definitions without rerunning the endpoint experiment.
source=(OWNED/'smooth-gap-proof.py').read_text()
exec(compile(source.split('\ntraces=')[0],str(OWNED/'smooth-gap-proof.py'),'exec'))

def padd(a,b):
    return [(a[i] if i<len(a) else F(0))+(b[i] if i<len(b) else F(0)) for i in range(max(len(a),len(b)))]
def pscale(a,s):return [x*s for x in a]
def psub(a,b):return padd(a,pscale(b,-1))
def pmul(a,b):
    out=[F(0)]*(len(a)+len(b)-1)
    for i,x in enumerate(a):
        for j,y in enumerate(b):out[i+j]+=x*y
    return out

def psum(terms):
    out=[F(0)]
    for p in terms:out=padd(out,p)
    return out

def bernstein(p):
    n=len(p)-1
    return [sum((p[i]*F(comb(k,i),comb(n,i)) for i in range(k+1)),F(0)) for k in range(n+1)]

def precise_polys(body,start,end):
    s=segment(body,start)
    if segment(body,end) is not s:raise ValueError('interval crosses a recorded motion boundary')
    if s['prescribed']:raise ValueError('exact polynomial proof is for Cayley segments')
    elapsed=start-F(f64(s['begin']));duration=end-start
    anchor=vf(s['position']);v=vf(s['linear_velocity'])
    position=[[anchor[k]+v[k]*elapsed,v[k]*duration] for k in range(3)]
    w,x,y,z=vf(s['orientation_wxyz']);ox,oy,oz=vf(s['drift_angular_velocity'])
    derivative=[-ox*x-oy*y-oz*z,ox*w+oy*z-oz*y,-ox*z+oy*w+oz*x,ox*y-oy*x+oz*w]
    q=[[v+d*elapsed/2,d*duration/2] for v,d in zip([w,x,y,z],derivative)]
    w,x,y,z=q
    ww,xx,yy,zz=[pmul(v,v) for v in q]
    xy,xz,yz,wx,wy,wz=[pmul(a,b) for a,b in [(x,y),(x,z),(y,z),(w,x),(w,y),(w,z)]]
    twice=lambda p:pscale(p,2)
    raw=[
      [psub(psub(padd(ww,xx),yy),zz),twice(padd(xy,wz)),twice(psub(xz,wy))],
      [twice(psub(xy,wz)),psub(psub(padd(ww,yy),xx),zz),twice(padd(yz,wx))],
      [twice(padd(xz,wy)),twice(psub(yz,wx)),psub(psub(padd(ww,zz),xx),yy)]]
    norm=psum([ww,xx,yy,zz])
    if min(bernstein(norm))<=0:raise ValueError('failed to certify positive quaternion norm')
    return position,raw,norm

def projected_corner(rotation_raw,corner,axis):
    return psum(pscale(rotation_raw[k][j],corner[k]*axis[j]) for k in range(3) for j in range(3))

traces=[json.loads(x) for x in (OWNED/'pair-trace.jsonl').read_text().splitlines()]
endpoint=json.loads((OWNED/'smooth-gap-proof.json').read_text())
results=[]
for item in endpoint['queries']:
    event=item['selected_event']
    if not event:continue
    if not item['probes'][0]['models']['precise_cayley_double_elapsed']['separated']:
        results.append({'record':item['record'],'event':event['chronological_event_in_step'],
                        'classification':'already-overlapping-at-query-start; no separated-plane proof attempted'})
        continue
    trace=traces[item['record']-1]
    a,b=trace['body_a'],trace['body_b']
    t0=F(f64(item['probes'][0]['time_bits']));t1=F(f64(item['probes'][-1]['time_bits']))
    pa,qa=smooth(a,t0);pb,qb=smooth(b,t0)
    ra,rb=rotation(qa),rotation(qb)
    axes=ra+rb+[cross(x,y) for x in ra for y in rb]
    axis=axes[item['probes'][0]['models']['precise_cayley_double_elapsed']['winning_axis']]
    if dot(sub(pb,pa),axis)<0:axis=mul(axis,-1)
    ap,ar,an=precise_polys(a,t0,t1);bp,br,bn=precise_polys(b,t0,t1)
    central=pmul(psum(pscale(psub(bp[j],ap[j]),axis[j]) for j in range(3)),pmul(an,bn))
    ha,hb=vf(a['shape']['half_extents']),vf(b['shape']['half_extents'])
    ac=[projected_corner(ar,[s[i]*ha[i] for i in range(3)],axis) for s in product([-1,1],repeat=3)]
    bc=[projected_corner(br,[s[i]*hb[i] for i in range(3)],axis) for s in product([-1,1],repeat=3)]
    minimum=None;all_positive=True;degree=0
    for av in ac:
        for bv in bc:
            numerator=padd(central,psub(pmul(bv,an),pmul(av,bn)))
            coefficients=bernstein(numerator);degree=max(degree,len(coefficients)-1)
            lower=min(coefficients)
            if minimum is None or lower<minimum:minimum=lower
            all_positive &= lower>0
    results.append({'record':item['record'],'event':event['chronological_event_in_step'],
                    'classification':'certified-separated-for-entire-closed-query-interval' if all_positive else 'not-certified',
                    'start_time_s':float(t0),'finish_time_s':float(t1),'fixed_axis_exact':[str(x) for x in axis],
                    'corner_pairs':64,'polynomial_degree':degree,
                    'minimum_bernstein_numerator':str(minimum.numerator),'minimum_bernstein_denominator':str(minimum.denominator),
                    'minimum_bernstein_value_unnormalized':float(minimum)})
    print(item['record'],event['chronological_event_in_step'],all_positive,float(minimum),flush=True)
result={'schema':'judas.exact-rational-obb-interval-separation.v1',
        'method':__doc__,
        'input_sha256':{n:hashlib.sha256((OWNED/n).read_bytes()).hexdigest() for n in ['smooth-gap-proof.py','smooth-gap-proof.json','pair-trace.jsonl']},
        'results':results}
(OWNED/'interval-separation-proof.json').write_text(json.dumps(result,indent=2)+'\n')
